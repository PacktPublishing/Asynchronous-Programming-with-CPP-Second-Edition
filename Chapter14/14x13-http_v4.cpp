// 14x13-http_v4.cpp
// =================
// Running example v4: channels as a bounded work queue. Sessions hand CPU-bound
// jobs to worker coroutines through a concurrent_channel with a fixed
// capacity; when it is full, try_send fails and the session answers 503
// instead of queueing without bound. Workers run as coroutines on the thread
// pool and reply through a per-request channel. Closing the job channel is
// the shutdown broadcast that ends every worker.
//
// Requires Boost 1.86+.
//   g++ -std=c++23 -I<boost>/include 14x13-http_v4.cpp -o 14x13-http_v4
// Test with: seq 1 32 | xargs -P 32 -I{} curl -s localhost:8080/primes/3000000

#include <boost/asio.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/beast.hpp>
#include <charconv>
#include <chrono>
#include <exception>
#include <format>
#include <memory>
#include <new>
#include <optional>
#include <print>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using boost::system::error_code;
using namespace std::chrono_literals;

constexpr auto idle_timeout = 15s;
constexpr auto write_timeout = 5s;
constexpr std::size_t queue_capacity = 4;  // jobs waiting for a worker

std::size_t count_primes(std::size_t n) {
    std::vector<bool> composite(n, false);
    std::size_t count = 0;
    for (std::size_t i = 2; i < n; ++i) {
        if (composite[i]) continue;
        ++count;
        for (std::size_t j = i * i; j < n; j += i) composite[j] = true;
    }
    return count;
}

// A task posted to a thread_pool must not let an exception escape, because
// Asio calls std::terminate if one does. Report the failure as a value.
std::optional<std::size_t> count_primes_checked(std::size_t n) noexcept {
    try {
        return count_primes(n);
    } catch (const std::bad_alloc&) {
        return std::nullopt;
    }
}

// The request decides how much work we do, so what it may ask for is bounded.
constexpr std::size_t max_primes_limit = 10'000'000;

// Parse "/primes/<n>". Returns no value when the target is not a primes
// request at all, and zero when it is one we refuse to serve.
std::optional<std::size_t> primes_limit(std::string_view target) {
    constexpr std::string_view prefix = "/primes/";
    if (!target.starts_with(prefix)) return std::nullopt;
    const char* first = target.data() + prefix.size();
    const char* last = target.data() + target.size();
    std::size_t n = 0;
    auto [ptr, ec] = std::from_chars(first, last, n);
    if (ec != std::errc{} || ptr != last) return 0;  // not a number
    return n > max_primes_limit ? 0 : n;             // more than we serve
}

// --- The work queue -------------------------------------------------------

using reply_channel = asio::experimental::concurrent_channel<void(error_code, std::size_t)>;

struct job {
    std::size_t limit;
    std::shared_ptr<reply_channel> reply;  // where the answer goes
};

using job_channel = asio::experimental::concurrent_channel<void(error_code, job)>;

error_code out_of_memory() { return boost::system::errc::make_error_code(boost::system::errc::not_enough_memory); }

// A worker coroutine: runs on the CPU pool, takes jobs until the channel closes.
asio::awaitable<void> worker(job_channel& jobs) {
    while (true) {
        auto [ec, j] = co_await jobs.async_receive(asio::as_tuple);  // [1]
        if (ec) break;                                               // channel closed: shut down
        auto count = count_primes_checked(j.limit);                  // on a pool thread
        // A failure travels back through the channel's error slot. Throwing
        // would end this worker for good and leave the session waiting forever.
        j.reply->try_send(count ? error_code{} : out_of_memory(),
                          count.value_or(0));  // capacity 1: never blocks
    }
}

// --- The HTTP side ----------------------------------------------------------

http::response<http::string_body> make_response(const http::request<http::string_body>& req, http::status status,
                                                std::string body) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::server, "asio-book/4.0");
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    res.body() = std::move(body);
    res.prepare_payload();
    return res;
}

// Report what actually went wrong. code().message() is the short text, while
// what() also carries the template instantiation that raised it, which for a
// Beast operation runs to a couple of thousand characters.
void log_session_error(std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (const boost::system::system_error& ex) {
        std::println(stderr, "session error: {}", ex.code().message());
    } catch (const std::exception& ex) {
        std::println(stderr, "session error: {}", ex.what());
    }
}

asio::awaitable<void> session(tcp::socket socket, job_channel& jobs) {
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    auto reply = std::make_shared<reply_channel>(stream.get_executor(), 1);
    while (true) {
        http::request<http::string_body> req;
        auto [ec, n] = co_await http::async_read(stream, buffer, req, asio::cancel_after(idle_timeout, asio::as_tuple));
        if (ec) break;  // client closed, idle deadline fired, or failure

        http::response<http::string_body> res;
        auto limit = primes_limit(req.target());
        if (!limit) {
            res = make_response(req, http::status::ok, std::format("Hello from {}\n", req.target()));
        } else if (*limit == 0) {
            res = make_response(req, http::status::bad_request, std::format("limit must be 1..{}\n", max_primes_limit));
        } else if (!jobs.try_send(error_code{}, job{*limit, reply})) {  // [2]
            res = make_response(req, http::status::service_unavailable, "busy: try again later\n");
        } else {
            auto [rec, count] = co_await reply->async_receive(  // [3]
                asio::as_tuple);
            res = rec ? make_response(req, http::status::internal_server_error, std::format("{}\n", rec.message()))
                      : make_response(req, http::status::ok, std::format("{} primes below {}\n", count, *limit));
        }
        auto [wec, written] =
            co_await http::async_write(stream, res, asio::cancel_after(write_timeout, asio::as_tuple));
        if (wec || !res.keep_alive()) break;
    }
    // The peer may already be gone, which is not an error worth reporting.
    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
}

asio::awaitable<void> listen(asio::io_context& io, tcp::endpoint endpoint, job_channel& jobs) {
    tcp::acceptor acceptor(io, endpoint);
    std::println("listening on port {}", endpoint.port());
    while (true) {
        auto [ec, socket] = co_await acceptor.async_accept(asio::make_strand(io), asio::as_tuple);
        if (ec == asio::error::operation_aborted) break;
        if (ec) {
            std::println(stderr, "accept: {}", ec.message());
            continue;
        }
        asio::co_spawn(socket.get_executor(), session(std::move(socket), jobs), [](std::exception_ptr e) {
            if (e) log_session_error(e);
        });
    }
    std::println("listener stopped; draining open sessions");
    jobs.close();  // [4]
}

int main() {
    asio::io_context io;
    asio::thread_pool cpu{2};
    job_channel jobs(cpu, queue_capacity);                                          // [5]
    for (int i = 0; i < 2; ++i) asio::co_spawn(cpu, worker(jobs), asio::detached);  // [6]

    auto listener = asio::make_strand(io);
    asio::cancellation_signal stop_listening;
    asio::co_spawn(listener, listen(io, {tcp::v4(), 8080}, jobs),
                   asio::bind_cancellation_slot(stop_listening.slot(), asio::detached));

    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait(asio::bind_executor(listener, [&](error_code ec, int sig) {
        if (ec) return;
        std::println("signal {}: draining; send again to stop now", sig);
        stop_listening.emit(asio::cancellation_type::terminal);
        signals.clear();
    }));

    std::vector<std::jthread> io_threads;
    for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) io_threads.emplace_back([&io] { io.run(); });
    io_threads.clear();
    cpu.join();
    std::println("server exited cleanly");
}
