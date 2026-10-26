// 12x10-scheduler_handle.cpp
// ==========================
// Schedulers: the handle onto a context. schedule(sched) is the scheduler's
// whole required interface; generic code constrains on the scheduler concept
// and runs on any resource; and read_env(get_scheduler) lets a pipeline
// discover the scheduler its caller is already running on.
// (Chapter 12, "Schedulers: The Handle onto a Context" and "Getting a
// Scheduler from the Environment".)
//
// C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 12x10-scheduler_handle.cpp
//       -o 12x10-scheduler_handle
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>
#include <string>
#include <thread>

namespace ex = stdexec;

// Any type modeling `scheduler` satisfies this template: a thread pool, a run
// loop, or a GPU.
template <ex::scheduler Sched>
ex::sender auto make_greeting(Sched sched) {
    return ex::schedule(sched) | ex::then([] { return std::string{"Hello from a scheduler"}; });
}

int main() {
    exec::static_thread_pool pool{2};
    ex::scheduler auto sched = pool.get_scheduler();

    // schedule(sched): a sender that completes on the resource sched names.
    ex::sender auto s = ex::schedule(sched);
    ex::sync_wait(std::move(s));

    // The same generic algorithm on two different resources.
    auto [on_pool] = ex::sync_wait(make_greeting(sched)).value();

    ex::run_loop loop;
    std::jthread driver([&loop] { loop.run(); });  // a thread drives the loop
    auto [on_loop] = ex::sync_wait(make_greeting(loop.get_scheduler())).value();
    loop.finish();
    std::println("{} (pool) / {} (run_loop)", on_pool, on_loop);

    // Discover the current scheduler instead of being told: under starts_on,
    // the environment's scheduler is the pool.
    ex::sender auto reschedule_here =
        ex::read_env(ex::get_scheduler)  // grab the current scheduler
        | ex::let_value([](ex::scheduler auto sc) { return ex::schedule(sc) | ex::then([] { return 1; }); });
    auto [one] = ex::sync_wait(ex::starts_on(sched, std::move(reschedule_here))).value();
    std::println("reschedule_here returned {}", one);
}
