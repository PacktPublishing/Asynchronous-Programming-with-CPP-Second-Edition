// 14x06-http_v2.cpp
// =================
// Running example v2: timeouts and graceful shutdown. Every read carries an
// idle deadline through cancel_after, every write a shorter one, and a
// signal_set turns SIGINT/SIGTERM into cancellation of the listener so that
// in-flight sessions drain (bounded by the idle deadline) and run() returns
// on its own. The signal set is then cleared, so a second signal terminates
// the process through the default disposition.
//
// Requires Boost 1.86+ (cancel_after, deferred default token).
//   g++ -std=c++23 -I<boost>/include 14x06-http_v2.cpp -o 14x06-http_v2
// Test with: curl -i http://localhost:8080/hello ; then press Ctrl-C.

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <exception>
#include <format>
#include <print>
#include <utility>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

constexpr auto idle_timeout = 15s;  // a client may stay silent this long
constexpr auto write_timeout = 5s;  // a response must be accepted this fast

http::response<http::string_body> handle(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, "asio-book/2.0");
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req.keep_alive());
    res.body() = std::format("Hello from {}\n", req.target());
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

asio::awaitable<void> session(tcp::socket socket) {
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    while (true) {
        http::request<http::string_body> req;
        auto [ec, n] =
            co_await http::async_read(stream, buffer, req, asio::cancel_after(idle_timeout, asio::as_tuple));  // [1]
        if (ec == asio::error::operation_aborted) {  // idle deadline
            std::println("session: idle for {}s, closing", idle_timeout.count());
            break;
        }
        if (ec) break;  // client closed, or the read failed

        auto res = handle(req);
        auto [wec, written] =
            co_await http::async_write(stream, res, asio::cancel_after(write_timeout, asio::as_tuple));  // [2]
        if (wec || !res.keep_alive()) break;
    }
    // The peer may already be gone, which is not an error worth reporting.
    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
}

asio::awaitable<void> listen(tcp::endpoint endpoint) {
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, endpoint);
    std::println("listening on port {}", endpoint.port());
    while (true) {
        auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple);
        if (ec == asio::error::operation_aborted) break;  // [3]
        if (ec) {
            std::println(stderr, "accept: {}", ec.message());
            continue;
        }
        asio::co_spawn(executor, session(std::move(socket)), [](std::exception_ptr e) {
            if (e) log_session_error(e);
        });
    }
    std::println("listener stopped; draining open sessions");
}

int main() {
    asio::io_context io;

    asio::cancellation_signal stop_listening;
    asio::co_spawn(io, listen({tcp::v4(), 8080}),
                   asio::bind_cancellation_slot(stop_listening.slot(),  // [4]
                                                asio::detached));

    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](boost::system::error_code ec, int sig) {
        if (ec) return;
        std::println("signal {}: draining; send again to stop now", sig);
        stop_listening.emit(asio::cancellation_type::terminal);  // [5]
        signals.clear();                                         // restore default handling: a second signal kills us
    });

    io.run();  // [6]
    std::println("server exited cleanly");
}
