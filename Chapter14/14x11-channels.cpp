// 14x11-channels.cpp
// ==================
// A channel connects a producer coroutine to a consumer coroutine. Its
// capacity is the amount of buffering between them: with capacity 0 every
// send waits for a receive (a rendezvous); with capacity N the producer can
// run N items ahead before it is suspended. That suspension is backpressure.
//
// Requires Boost 1.78+ (experimental::channel).
//   g++ -std=c++23 -I<boost>/include 14x11-channels.cpp

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <chrono>
#include <print>

namespace asio = boost::asio;
using boost::system::error_code;
using namespace std::chrono_literals;

using channel = asio::experimental::channel<void(error_code, int)>;

asio::awaitable<void> producer(channel& ch, int count) {
    for (int i = 1; i <= count; ++i) {
        std::println("producer: sending {}", i);
        co_await ch.async_send(error_code{}, i);  // [1]
        std::println("producer: {} accepted", i);
    }
    ch.close();  // [2]
}

asio::awaitable<void> consumer(channel& ch) {
    asio::steady_timer pace(co_await asio::this_coro::executor);
    while (true) {
        auto [ec, value] = co_await ch.async_receive(asio::as_tuple);  // [3]
        if (ec) {                                                      // closed and drained
            std::println("consumer: channel closed");
            break;
        }
        std::println("consumer: got {}", value);
        pace.expires_after(30ms);  // a slow consumer
        co_await pace.async_wait();
    }
}

int main() {
    asio::io_context io;
    channel ch(io, 2);  // [4] capacity 2
    asio::co_spawn(io, producer(ch, 5), asio::detached);
    asio::co_spawn(io, consumer(ch), asio::detached);
    io.run();
}
