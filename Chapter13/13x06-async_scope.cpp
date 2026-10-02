// 13x06-async_scope.cpp
// Owning background work: an async_scope owns the operations spawned into it
// and provides on_empty() as a structured join point, so the pool is safe to
// destroy. This is the disciplined alternative to start_detached.
//
// Two ways in: spawn() launches a sender and returns nothing, while
// spawn_future() launches one and hands back a second sender carrying the
// result. Either way the scope owns the work, and on_empty() joins it.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x06-async_scope.cpp
//       -o 13x06-async_scope
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <atomic>
#include <print>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>

namespace ex = stdexec;

int main() {
    exec::async_scope scope;
    exec::static_thread_pool pool{4};
    ex::scheduler auto sched = pool.get_scheduler();

    std::atomic<int> done{0};

    // spawn: launch into the scope and return nothing. The result, if any, is
    // discarded; the scope keeps the operation alive.
    scope.spawn(ex::schedule(sched) | ex::then([&] { done.fetch_add(1); }));
    scope.spawn(ex::schedule(sched) | ex::then([&] { done.fetch_add(1); }));

    // spawn_future: launch into the scope and hand back a second sender that
    // delivers the result once it arrives. Despite the name it is not a
    // std::future and has no get(), so we consume it like any other sender.
    // Dropping it instead would leave the work running under the scope.
    ex::sender auto result = scope.spawn_future(
          ex::schedule(sched)
        | ex::then([&] { done.fetch_add(1); return 42; }));

    auto [value] = ex::sync_wait(std::move(result)).value();
    std::println("spawn_future delivered {}", value);

    // Join: wait for every spawned task before the pool is destroyed.
    ex::sync_wait(scope.on_empty());

    std::println("spawned tasks completed: {}", done.load());
}
