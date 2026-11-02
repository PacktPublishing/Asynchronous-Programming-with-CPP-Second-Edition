// 14x15-use_sender.cpp
// ====================
// use_sender: an Asio completion token that turns any asynchronous operation
// into a std::execution sender. A timer wait, a socket read, and a post() each
// become senders; a when_any race cancels a pending read through the
// receiver's stop token; an error completes on the error channel.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec and Boost 1.86+.
//   g++ -std=c++23 -I<stdexec>/include -I<boost>/include 14x15-use_sender.cpp

#include <array>
#include <chrono>
#include <exec/timed_thread_scheduler.hpp>
#include <exec/when_any.hpp>
#include <optional>
#include <print>
#include <string_view>
#include <thread>

#include "asio_bridge.hpp"

using namespace std::chrono_literals;

int main() {
    asio::io_context io;
    auto work = asio::make_work_guard(io);
    std::jthread io_thread([&] { io.run(); });

    // [1] A timer wait is a sender that completes with no value.
    asio::steady_timer timer(io, 50ms);
    ex::sync_wait(timer.async_wait(use_sender) | ex::then([] { std::println("timer expired"); }));

    // [2] A socket read is a sender that completes with the byte count.
    asio::local::stream_protocol::socket reader(io), writer(io);
    asio::local::connect_pair(reader, writer);
    std::array<char, 64> buf{};

    asio::write(writer, asio::buffer(std::string_view{"hello, sender"}));
    auto [n] = ex::sync_wait(reader.async_read_some(asio::buffer(buf), use_sender) | ex::then([&](std::size_t n) {
                                 std::println("read {} bytes: {}", n, std::string_view(buf.data(), n));
                                 return n;
                             }))
                   .value();

    // [3] A pending read raced against a deadline: the timer wins, when_any
    //     requests stop, and the read is cancelled through its slot.
    exec::timed_thread_context timer_ctx;
    auto deadline =
        exec::schedule_after(timer_ctx.get_scheduler(), 20ms) | ex::then([] { return std::optional<std::size_t>{}; });
    auto read = reader.async_read_some(asio::buffer(buf), use_sender) |
                ex::then([](std::size_t n) { return std::optional{n}; });

    auto [raced] = ex::sync_wait(exec::when_any(std::move(read), std::move(deadline))).value();
    std::println("read timed out: {}", !raced.has_value());
    std::println("socket still open: {}", reader.is_open());

    // [4] An error completes on the error channel and surfaces at sync_wait.
    writer.close();
    try {
        ex::sync_wait(reader.async_read_some(asio::buffer(buf), use_sender));
        std::println("unexpected: read succeeded");
    } catch (const boost::system::system_error& e) {
        std::println("read failed: {}", e.code().message());
    }

    // [5] post() with use_sender is schedule() by another name.
    auto [tid] =
        ex::sync_wait(asio::post(io, use_sender) | ex::then([] { return std::this_thread::get_id(); })).value();
    std::println("post completed on io thread: {}", tid == io_thread.get_id());

    work.reset();
    (void)n;
}
