// 14x17-asio_in_task.cpp
// ======================
// Asio operations awaited from a std::execution coroutine. Because use_sender
// makes every Asio operation a sender, and ex::task (std::execution::task) can
// co_await any sender, a task can read a socket, run a step on a CPU pool, and
// wait on a timer with the same co_await. After every co_await the task
// resumes on the scheduler it was started on (scheduler affinity). Cancelling
// the task cancels the Asio operation it is waiting on.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec and Boost 1.86+.
//   g++ -std=c++23 -I<stdexec>/include -I<boost>/include 14x17-asio_in_task.cpp

#include <array>
#include <cctype>
#include <chrono>
#include <exec/static_thread_pool.hpp>
#include <exec/timed_thread_scheduler.hpp>
#include <exec/when_any.hpp>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>

#include "asio_bridge.hpp"

using namespace std::chrono_literals;
using socket_t = asio::local::stream_protocol::socket;

// Read a line, "process" it on the CPU pool, and wait on an Asio timer.
ex::task<std::string> serve(socket_t& sock, asio_scheduler io, ex::scheduler auto cpu) {
    std::array<char, 64> buf{};
    auto n = co_await sock.async_read_some(asio::buffer(buf),  // [1]
                                           use_sender);
    std::string text(buf.data(), n);

    text = co_await ex::starts_on(cpu, ex::just(std::move(text))  // [2]
                                           | ex::then([](std::string s) {
                                                 for (auto& c : s) c = static_cast<char>(std::toupper(c));
                                                 return s;
                                             }));

    asio::steady_timer pause(io.get_executor(), 10ms);  // [3]
    co_await pause.async_wait(use_sender);              // [4]
    co_return text;
}

int main() {
    asio::io_context io;
    auto work = asio::make_work_guard(io);
    std::jthread io_thread([&] { io.run(); });
    exec::static_thread_pool pool{2};
    asio_scheduler io_sched{io.get_executor()};

    socket_t reader(io), writer(io);
    asio::local::connect_pair(reader, writer);

    // A message is waiting: the task completes with the processed text.
    // The task runs on the run_loop of the thread that calls sync_wait.
    asio::write(writer, asio::buffer(std::string_view{"hello from asio"}));
    auto [reply] = ex::sync_wait(serve(reader, io_sched, pool.get_scheduler())).value();
    std::println("reply: {}", reply);

    // Nothing is waiting: race the task against a deadline. The timer wins,
    // the task is stopped, and the read it was suspended on is cancelled.
    exec::timed_thread_context timer_ctx;
    auto deadline =
        exec::schedule_after(timer_ctx.get_scheduler(), 30ms) | ex::then([] { return std::optional<std::string>{}; });
    auto attempt = serve(reader, io_sched, pool.get_scheduler()) |
                   ex::then([](std::string s) { return std::optional{std::move(s)}; });
    auto [raced] = ex::sync_wait(exec::when_any(std::move(attempt), std::move(deadline))).value();
    std::println("second attempt timed out: {}", !raced.has_value());

    work.reset();
}
