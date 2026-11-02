// 14x05-http_v1.cpp
// =================
// Running example v1: an HTTP service built from Boost.Beast and Asio
// coroutines. listen() accepts connections and spawns one session() coroutine
// per client; session() reads requests and writes responses until the client
// closes. The coroutine frame owns the socket, so no shared_from_this.
//
// Requires Boost 1.86+ (deferred is the default completion token).
//   g++ -std=c++23 -I<boost>/include 14x05-http_v1.cpp -o 14x05-http_v1
// Test with: curl -i http://localhost:8080/hello

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <exception>
#include <format>
#include <print>
#include <utility>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// The application logic: a plain function from request to response.
http::response<http::string_body> handle(const http::request<http::string_body>& req) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, "asio-book/1.0");
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

// One coroutine per connection. The frame owns the stream and the buffer.
asio::awaitable<void> session(tcp::socket socket) {
    beast::tcp_stream stream(std::move(socket));  // [1]
    beast::flat_buffer buffer;
    while (true) {
        http::request<http::string_body> req;
        auto [ec, n] = co_await http::async_read(  // [2]
            stream, buffer, req, asio::as_tuple);
        if (ec) break;  // client closed, or the read failed

        auto res = handle(req);
        auto [wec, written] = co_await http::async_write(  // [3]
            stream, res, asio::as_tuple);
        if (wec || !res.keep_alive()) break;
    }
    // The peer may already be gone, which is not an error worth reporting. [4]
    boost::system::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);  // [4]
}

// The acceptor loop: one coroutine for the lifetime of the server.
asio::awaitable<void> listen(tcp::endpoint endpoint) {
    auto executor = co_await asio::this_coro::executor;  // [5]
    tcp::acceptor acceptor(executor, endpoint);
    std::println("listening on port {}", endpoint.port());
    while (true) {
        tcp::socket socket = co_await acceptor.async_accept();  // [6]
        asio::co_spawn(executor, session(std::move(socket)),    // [7]
                       [](std::exception_ptr e) {
                           if (e) log_session_error(e);
                       });
    }
}

int main() {
    asio::io_context io;
    asio::co_spawn(io, listen({tcp::v4(), 8080}), asio::detached);
    io.run();
}
