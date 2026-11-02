// 14x08-timeouts.cpp
// ==================
// Two spellings of a timeout. cancel_after attaches a deadline to a single
// operation; operator|| races a whole coroutine against a timer and cancels
// the loser. The second is Asio's when_any.
//
// Requires Boost 1.86+ (cancel_after) and 1.77+ (awaitable_operators).
//   g++ -std=c++23 -I<boost>/include 14x08-timeouts.cpp

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <print>
#include <variant>

namespace asio = boost::asio;
using namespace std::chrono_literals;
using namespace asio::experimental::awaitable_operators;

// Stands in for a slow remote call.
asio::awaitable<int> slow_call(std::chrono::milliseconds latency) {
    asio::steady_timer t(co_await asio::this_coro::executor, latency);
    co_await t.async_wait();
    co_return 42;
}

asio::awaitable<void> demo() {
    auto executor = co_await asio::this_coro::executor;

    // [1] A deadline on one operation.
    asio::steady_timer t(executor, 500ms);
    auto [ec] = co_await t.async_wait(asio::cancel_after(50ms, asio::as_tuple));
    std::println("cancel_after: {}", ec == asio::error::operation_aborted ? "timed out" : "completed");

    // [2] A deadline on a whole coroutine: race it against a timer.
    asio::steady_timer deadline(executor, 50ms);
    std::variant<int, std::monostate> r = co_await (slow_call(500ms) || deadline.async_wait(asio::use_awaitable));
    std::println("operator||  : {}", r.index() == 0 ? "value" : "timed out");

    // [3] The same race when the work is fast enough.
    deadline.expires_after(500ms);
    r = co_await (slow_call(50ms) || deadline.async_wait(asio::use_awaitable));
    std::println("operator||  : {} = {}", r.index() == 0 ? "value" : "timed out", r.index() == 0 ? std::get<0>(r) : -1);
}

int main() {
    asio::io_context io;
    asio::co_spawn(io, demo(), asio::detached);
    io.run();
}
