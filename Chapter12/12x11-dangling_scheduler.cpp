// 12x11-dangling_scheduler.cpp
// ============================
// A scheduler is a non-owning handle, like a raw pointer or a string_view.
// make_dangling() returns a scheduler whose context is destroyed at the
// closing brace: the code compiles, and any use of the result is undefined
// behavior. It is defined here so the chapter's listing compiles, and it is
// never called. The fix is to own the context in a scope that outlives every
// copy of the scheduler and every piece of work scheduled through it.
// (Chapter 12, "Best Practices and Pitfalls".)
//
// C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 12x11-dangling_scheduler.cpp
//       -o 12x11-dangling_scheduler
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

ex::scheduler auto make_dangling() {
    exec::static_thread_pool pool{2};
    return pool.get_scheduler();
    // BUG: pool is destroyed at the closing brace,
    //      so the returned scheduler now dangles.
}

// The fix: the context is owned by the caller, and the scheduler is borrowed.
ex::sender auto work_on(ex::scheduler auto sched) {
    return ex::schedule(sched) | ex::then([] { return 42; });
}

int main() {
    exec::static_thread_pool pool{2};  // owned here, outlives the work
    auto [value] = ex::sync_wait(work_on(pool.get_scheduler())).value();
    std::println("work on a living pool returned {}", value);

    // Do not do this: the pool behind `bad` no longer exists.
    // auto bad = make_dangling();
    // ex::sync_wait(ex::schedule(bad));       // undefined behavior
}
