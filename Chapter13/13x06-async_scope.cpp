// 13x06-async_scope.cpp
// =====================
// Owning background work: a counting_scope owns the operations spawned into it
// and provides join() as a structured join point, so the pool is safe to
// destroy. This is the disciplined alternative to start_detached.
//
// Two ways in: spawn() launches a sender and returns nothing, while
// spawn_future() launches one and hands back a second sender carrying the
// result. Either way the scope owns the work, and join() waits for it.
//
// C++26 std::execution (P2300, async scopes from P3149). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x06-async_scope.cpp
//       -o 13x06-async_scope
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <atomic>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

std::atomic<int> done{0};
void do_background_work() noexcept { done.fetch_add(1); }
void do_more_work() noexcept { done.fetch_add(1); }

int main() {
    ex::counting_scope scope;
    exec::static_thread_pool pool{4};
    ex::scheduler auto sched = pool.get_scheduler();

    // spawn: launch into the scope and return nothing. A spawned sender must
    // not be able to fail, because its error would have nowhere to go: spawn
    // rejects it at compile time. The lambdas are noexcept for that reason.
    ex::spawn(ex::schedule(sched) | ex::then([]() noexcept { do_background_work(); }), scope.get_token());
    ex::spawn(ex::schedule(sched) | ex::then([]() noexcept { do_more_work(); }), scope.get_token());

    // spawn_future: launch into the scope and hand back a second sender that
    // delivers the result once it arrives. Despite the name it is not a
    // std::future and has no get(), so we consume it like any other sender.
    // Dropping it instead would ask the spawned work to stop.
    ex::sender auto result = ex::spawn_future(ex::schedule(sched) | ex::then([] {
                                                  done.fetch_add(1);
                                                  return 42;
                                              }),
                                              scope.get_token());

    auto [value] = ex::sync_wait(std::move(result)).value();
    std::println("spawn_future delivered {}", value);

    // Wait for every spawned operation and join
    // before the pool is destroyed.
    // Destroying a scope that has had work and was never joined calls
    // std::terminate.
    ex::sync_wait(scope.join());

    std::println("spawned tasks completed: {}", done.load());
}
