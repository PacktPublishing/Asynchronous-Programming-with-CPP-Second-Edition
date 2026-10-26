// 12x03-run_loop.cpp
// ==================
// run_loop is a manually driven, single-threaded execution context. A worker
// thread pumps the queue with run(); finish() lets run() return once drained.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x03-run_loop.cpp
//       -o 12x03-run_loop
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <cstdio>
#include <stdexec/execution.hpp>
#include <thread>

namespace ex = stdexec;

int main() {
    ex::run_loop loop;

    // A worker thread drives the loop, executing whatever is queued.
    std::jthread driver([&loop] { loop.run(); });

    ex::scheduler auto sched = loop.get_scheduler();

    ex::sync_wait(ex::schedule(sched) | ex::then([] { std::puts("ran on the run_loop thread"); }));

    loop.finish();  // tell run() to return once the queue drains
}
