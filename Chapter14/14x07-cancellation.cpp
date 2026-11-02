// 14x07-cancellation.cpp
// ======================
// Per-operation cancellation: a cancellation_signal emits a request of a
// given type into the slot bound to one operation. A coroutine spawned with
// a slot forwards that request to whatever it is awaiting, and can inspect
// or filter it through its cancellation state.
//
// Requires Boost 1.77+ (per-operation cancellation).
//   g++ -std=c++23 -I<boost>/include 14x07-cancellation.cpp

#include <boost/asio.hpp>
#include <chrono>
#include <exception>
#include <print>

namespace asio = boost::asio;
using boost::system::error_code;
using namespace std::chrono_literals;

void report(const char *label, error_code ec, asio::cancellation_type_t bits) {
    std::println("{}: {} (cancelled bits = {:#x})", label,
                 ec == asio::error::operation_aborted ? "cancelled" : "expired", static_cast<unsigned>(bits));
}

// A coroutine spawned with a slot. Its cancellation state admits terminal
// requests only, unless we widen it; and once a request has been delivered,
// the next co_await throws unless the state is reset.
asio::awaitable<void> guarded_wait() {
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);

    // Admit every kind of cancellation for the first wait.
    co_await asio::this_coro::reset_cancellation_state(  // [1]
        asio::enable_total_cancellation());
    timer.expires_after(1s);
    auto [ec] = co_await timer.async_wait(asio::as_tuple);
    auto state = co_await asio::this_coro::cancellation_state;  // [2]
    report("first wait", ec, state.cancelled());

    // Reset: clears the delivered request (so the next co_await does not
    // throw) and admits terminal requests only from here on.
    co_await asio::this_coro::reset_cancellation_state(  // [3]
        asio::enable_terminal_cancellation());
    timer.expires_after(200ms);
    auto [ec2] = co_await timer.async_wait(asio::as_tuple);
    state = co_await asio::this_coro::cancellation_state;
    report("second wait", ec2, state.cancelled());
}

int main() {
    asio::io_context io;

    // [4] One operation, one slot: cancel a timer wait before it expires.
    asio::steady_timer timer(io, 1s);
    asio::cancellation_signal signal;
    timer.async_wait(asio::bind_cancellation_slot(
        signal.slot(), [](error_code ec) { std::println("timer      : {}", ec.message()); }));
    asio::post(io, [&] { signal.emit(asio::cancellation_type::total); });
    io.run();
    io.restart();

    // [5] A coroutine spawned with a slot forwards requests to its awaits.
    asio::cancellation_signal coroutine_signal;
    asio::co_spawn(io, guarded_wait(), asio::bind_cancellation_slot(coroutine_signal.slot(), asio::detached));

    asio::steady_timer at_100ms(io, 100ms), at_200ms(io, 200ms);
    at_100ms.async_wait([&](error_code) {  // during the first wait
        coroutine_signal.emit(asio::cancellation_type::partial);
    });
    at_200ms.async_wait([&](error_code) {  // during the second wait
        coroutine_signal.emit(asio::cancellation_type::partial);
    });
    io.run();
}
