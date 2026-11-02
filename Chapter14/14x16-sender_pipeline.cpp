// 14x16-sender_pipeline.cpp
// =========================
// The HTTP service's request handling written as a std::execution pipeline.
// Reads and writes are Asio operations turned into senders by use_sender;
// the CPU-bound step runs on a static_thread_pool through continues_on; and
// the response is written back on the I/O thread. One connection is served
// per pipeline, driven from an ordinary Asio coroutine.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec and Boost 1.86+.
//   g++ -std=c++23 -I<stdexec>/include -I<boost>/include 14x16-sender_pipeline.cpp
// Test with: curl http://localhost:8080/primes/2000000

#include <boost/beast.hpp>
#include <charconv>
#include <exception>
#include <exec/static_thread_pool.hpp>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string_view>
#include <thread>
#include <vector>

#include "asio_bridge.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

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

// A peer that disappears is the ordinary end of an exchange, not an error.
bool peer_gone(boost::system::error_code ec) {
    return ec == http::error::end_of_stream || ec == asio::error::connection_reset || ec == asio::error::broken_pipe ||
           ec == asio::error::operation_aborted;
}

// Everything one exchange needs, owned for the pipeline's lifetime.
struct exchange {
    beast::tcp_stream stream;
    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    http::response<http::string_body> response;
    explicit exchange(tcp::socket socket) : stream(std::move(socket)) {}
};

// One request-response as a sender: read on I/O, compute on CPU, write on I/O.
ex::sender auto serve_one(std::shared_ptr<exchange> x, asio_scheduler io, ex::scheduler auto cpu) {
    return http::async_read(x->stream, x->buffer, x->request, use_sender)  // [1]
           | ex::continues_on(cpu)                                         // [2]
           | ex::then([x](std::size_t) {
                 auto& req = x->request;
                 auto limit = primes_limit(req.target());
                 auto status = http::status::ok;
                 std::string body;
                 if (!limit) {
                     body = std::format("Hello from {}\n", req.target());
                 } else if (*limit == 0) {
                     status = http::status::bad_request;
                     body = std::format("limit must be 1..{}\n", max_primes_limit);
                 } else {
                     // A throw here becomes an error completion, not a terminate.
                     body = std::format("{} primes below {}\n", count_primes(*limit), *limit);
                 }
                 x->response = {status, req.version()};
                 x->response.set(http::field::server, "asio-book/senders");
                 x->response.set(http::field::content_type, "text/plain");
                 x->response.keep_alive(false);
                 x->response.body() = std::move(body);
                 x->response.prepare_payload();
             }) |
           ex::continues_on(io)  // [3]
           | ex::let_value([x] {
                 return http::async_write(x->stream, x->response, use_sender);  // [4]
             }) |
           ex::then([x](std::size_t) {
               boost::system::error_code ignored;
               x->stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
           });
}

asio::awaitable<void> listen(asio::io_context& io, tcp::endpoint endpoint, exec::static_thread_pool& pool,
                             ex::counting_scope& scope) {
    tcp::acceptor acceptor(io, endpoint);
    std::println("listening on port {}", endpoint.port());
    asio_scheduler io_sched{io.get_executor()};
    while (true) {
        auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple);
        if (ec == asio::error::operation_aborted) break;
        if (ec) continue;
        auto x = std::make_shared<exchange>(std::move(socket));
        ex::spawn(serve_one(x, io_sched, pool.get_scheduler())  // [5]
                      | ex::upon_error([](std::exception_ptr e) noexcept {
                            try {
                                std::rethrow_exception(e);
                            } catch (const boost::system::system_error& ex) {
                                // A peer that disappears ends the exchange; it is
                                // not a failure of ours. code().message() is the
                                // short text, what() adds the instantiation.
                                if (peer_gone(ex.code())) return;
                                std::println(stderr, "pipeline error: {}", ex.code().message());
                            } catch (const std::exception& ex) {
                                std::println(stderr, "pipeline error: {}", ex.what());
                            }
                        }),
                  scope.get_token());
    }
    std::println("listener stopped");
}

int main() {
    asio::io_context io;
    auto work = asio::make_work_guard(io);
    std::jthread io_thread([&] { io.run(); });

    exec::static_thread_pool pool{2};
    ex::counting_scope scope;  // [6]

    asio::cancellation_signal stop_listening;
    auto listener = asio::co_spawn(io, listen(io, {tcp::v4(), 8080}, pool, scope),
                                   asio::bind_cancellation_slot(stop_listening.slot(), asio::use_future));

    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](boost::system::error_code ec, int sig) {
        if (ec) return;
        std::println("signal {}: shutting down", sig);
        stop_listening.emit(asio::cancellation_type::terminal);
        signals.clear();
    });

    listener.get();               // the listener has finished
    ex::sync_wait(scope.join());  // [7] drain in-flight pipelines
    work.reset();                 // let run() return
    io_thread.join();
    std::println("server exited cleanly");
}
