// 13x01-motivating_pipeline.cpp
// A small pipeline: start on a pool, read a value, transform it, and recover
// from any error on the value path with upon_error. Nothing runs until
// sync_wait connects a receiver and starts it.
//
// PREVIEW: C++26 std::execution (P2300). Requires the NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x01-motivating_pipeline.cpp
//       -o 13x01-motivating_pipeline
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <print>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

int read_sensor() { return 3; }   // pretend this does I/O

int main() {
    exec::static_thread_pool pool{4};
    ex::scheduler auto sched = pool.get_scheduler();

    ex::sender auto pipeline =
          ex::schedule(sched)                            // start on the pool
        | ex::then([]        { return read_sensor(); })  // produce a value
        | ex::then([](int v) { return v * 100; })        // transform it
        | ex::upon_error([](std::exception_ptr) { return -1; });  // recover

    auto [value] = ex::sync_wait(std::move(pipeline)).value();
    std::println("value = {}", value);
}
