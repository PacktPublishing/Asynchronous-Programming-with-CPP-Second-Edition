// 12x01-dispatch_work.cpp
// =======================
// A first look at the execution framework: run work on a thread pool and block
// for the result. Mirrors "A First Look: Dispatching Work Somewhere".
//
// PREVIEW: C++26 std::execution (P2300). Requires the NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec); no shipping standard
// library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x01-dispatch_work.cpp
//       -o 12x01-dispatch_work
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
    exec::static_thread_pool pool{4};                 // [1] an execution context
    ex::scheduler auto sched = pool.get_scheduler();  // [2] a handle onto it

    ex::sender auto work = ex::schedule(sched)            // [3] start on the pool
                           | ex::then([] { return 21; })  // [4] run a function there
                           | ex::then([](int x) { return x * 2; });

    auto [result] = ex::sync_wait(std::move(work)).value();  // [5] block for it
    std::println("result = {}", result);                     // prints: result = 42
}
