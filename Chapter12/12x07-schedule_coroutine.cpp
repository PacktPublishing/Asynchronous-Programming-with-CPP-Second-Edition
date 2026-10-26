// 12x07-schedule_coroutine.cpp
// ============================
// Scheduling a coroutine. A std::execution::task runs on the scheduler it was
// started on, and after every co_await it resumes there again (scheduler
// affinity). So co_await schedule(other) does not move the task: the scheduled
// operation completes on `other`, and the task comes straight back. To run the
// whole task on a pool, start it there with starts_on. To run one step on
// another resource, await a sender that runs there.
//
// PREVIEW: C++26 std::execution (P2300 and P3552). Requires NVIDIA stdexec.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x07-schedule_coroutine.cpp
//       -o 12x07-schedule_coroutine

#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>
#include <thread>

namespace ex = stdexec;

int expensive_computation() { return 42; }

ex::task<int> compute(ex::scheduler auto other, std::thread::id& step_thread) {
    const auto start = std::this_thread::get_id();

    co_await ex::schedule(other);  // [1]
    const bool still_home = std::this_thread::get_id() == start;

    int result = co_await ex::starts_on(other, ex::just() | ex::then([&] {  // [2]
                                                   step_thread = std::this_thread::get_id();
                                                   return expensive_computation();
                                               }));

    std::println("after co_await schedule(other), still on the start thread: {}", still_home);
    std::println("after co_await starts_on(other, ...), back on the start thread: {}",
                 std::this_thread::get_id() == start);
    co_return result;
}

int main() {
    exec::static_thread_pool home{1};
    exec::static_thread_pool other{1};

    auto [other_thread] =
        ex::sync_wait(ex::schedule(other.get_scheduler()) | ex::then([] { return std::this_thread::get_id(); }))
            .value();

    std::thread::id step_thread;
    auto [result] = ex::sync_wait(ex::starts_on(home.get_scheduler(),  // [3]
                                                compute(other.get_scheduler(), step_thread)))
                        .value();
    std::println("the step ran on the other pool: {}", step_thread == other_thread);
    std::println("coroutine returned {}", result);
}
