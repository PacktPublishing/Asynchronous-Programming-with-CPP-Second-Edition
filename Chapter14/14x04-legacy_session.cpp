// 14x04-legacy_session.cpp
// ========================
// The callback-era connection object: a session that keeps itself alive with
// enable_shared_from_this while its asynchronous operations are in flight.
// Every pre-coroutine Asio codebase has one of these; compare it with the
// coroutine session in 14x05, where the frame owns the socket.
//
//   g++ -std=c++23 -I<boost>/include 14x04-legacy_session.cpp

#include <array>
#include <boost/asio.hpp>
#include <memory>
#include <print>
#include <string_view>
#include <utility>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using boost::system::error_code;

class session : public std::enable_shared_from_this<session> {  // [1]
   public:
    explicit session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }

   private:
    void do_read() {
        auto self = shared_from_this();                                                            // [2]
        socket_.async_read_some(asio::buffer(data_), [this, self](error_code ec, std::size_t n) {  // [3]
            if (!ec) do_write(n);
            // On error the lambda dies, self with it, and the session
            // is destroyed once no other operation holds a copy.
        });
    }
    void do_write(std::size_t n) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(data_, n), [this, self](error_code ec, std::size_t) {
            if (!ec) do_read();  // [4]
        });
    }
    tcp::socket socket_;
    std::array<char, 1024> data_{};
};

class server {
   public:
    server(asio::io_context& io, unsigned short port) : acceptor_(io, {tcp::v4(), port}) { do_accept(); }

   private:
    void do_accept() {
        acceptor_.async_accept([this](error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<session>(std::move(socket))->start();
            do_accept();
        });
    }
    tcp::acceptor acceptor_;
};

// An in-process client so the example can check itself.
asio::awaitable<void> client(unsigned short port) {
    tcp::socket socket(co_await asio::this_coro::executor);
    co_await socket.async_connect({asio::ip::make_address("127.0.0.1"), port});
    co_await asio::async_write(socket, asio::buffer(std::string_view{"ping"}));
    std::array<char, 16> reply{};
    auto n = co_await socket.async_read_some(asio::buffer(reply));
    std::println("echoed: {}", std::string_view(reply.data(), n));
}

int main() {
    asio::io_context io;
    server srv(io, 8081);
    asio::co_spawn(io, client(8081), [&](std::exception_ptr) { io.stop(); });
    io.run();
}
