// 14x01-asio_model.cpp
// ====================
// The Asio model in one program: an execution context (io_context), two I/O
// objects (a timer and a signal set), an asynchronous operation with a
// completion handler, a coroutine spawned onto the context, and the event
// loop that runs them all. Every later example is this shape scaled up.
//
// Requires Boost 1.86+ (deferred default completion token).
//   g++ -std=c++23 -I<boost>/include 14x01-asio_model.cpp -o 14x01-asio_model

#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <exception>
#include <print>

namespace asio = boost::asio;
using namespace std::chrono_literals;

// A coroutine that ticks n times. Its executor is the context it was spawned on.
asio::awaitable<void> ticker(int n) {
    auto executor = co_await asio::this_coro::executor;  // [4]
    asio::steady_timer timer(executor);
    for (int i = 1; i <= n; ++i) {
        timer.expires_after(200ms);
        co_await timer.async_wait();  // [5]
        std::println("tick {}", i);
    }
}

int main() {
    asio::io_context io;  // [1]

    asio::signal_set signals(io, SIGINT, SIGTERM);                   // [2]
    signals.async_wait([&](boost::system::error_code ec, int sig) {  // [3]
        if (!ec) {
            std::println("signal {}: stopping", sig);
            io.stop();
        }
    });

    asio::co_spawn(io, ticker(3), [&](std::exception_ptr) {  // [6]
        signals.cancel();                                    // no more work: let run() return
    });

    std::println("running the event loop");
    io.run();  // [7]
    std::println("event loop finished");
}
