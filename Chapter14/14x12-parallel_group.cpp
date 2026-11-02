// 14x12-parallel_group.cpp
// ========================
// Waiting for several operations at once. operator&& joins two awaitables
// (when_all); make_parallel_group with wait_for_one races any number of
// operations and cancels the rest (when_any), returning the completion order.
//
// Requires Boost 1.77+ (parallel_group, awaitable_operators).
//   g++ -std=c++23 -I<boost>/include 14x12-parallel_group.cpp

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <chrono>
#include <print>
#include <string>

namespace asio = boost::asio;
using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;

// A replica that answers after a given latency.
asio::awaitable<std::string> query_replica(std::string name, std::chrono::milliseconds latency) {
    asio::steady_timer t(co_await asio::this_coro::executor, latency);
    co_await t.async_wait();
    co_return name;
}

asio::awaitable<void> demo() {
    auto executor = co_await asio::this_coro::executor;

    // [1] Join: both must finish; results come back as a tuple.
    auto [a, b] = co_await (query_replica("eu", 30ms) && query_replica("us", 10ms));
    std::println("joined: {} and {}", a, b);

    // [2] Race: the first to answer wins; the others are cancelled.
    auto [order, e1, r1, e2, r2, e3, r3] = co_await asio::experimental::make_parallel_group(
                                               asio::co_spawn(executor, query_replica("eu", 30ms), asio::deferred),
                                               asio::co_spawn(executor, query_replica("us", 10ms), asio::deferred),
                                               asio::co_spawn(executor, query_replica("ap", 50ms), asio::deferred))
                                               .async_wait(asio::experimental::wait_for_one(), asio::deferred);

    std::println("winner: index {}", order[0]);  // winner: index 1 -> us
    (void)r1;
    (void)r2;
    (void)r3;
}

int main() {
    asio::io_context io;
    asio::co_spawn(io, demo(), asio::detached);
    io.run();
}
