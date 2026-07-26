// 12x03-run_loop.cpp
// ==================
// run_loop is a manually driven, single-threaded execution context.
// A worker thread pumps the queue with run().
// finish() lets run() return once drained.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x03-run_loop.cpp
//       -o 12x03-run_loop

#include <cstdio>
#include <thread>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
  ex::run_loop loop;

  // A worker thread drives the loop, executing whatever is queued.
  std::jthread driver([&loop] { loop.run(); });

  ex::scheduler auto sched = loop.get_scheduler();

  ex::sync_wait(ex::schedule(sched) |
                ex::then([] {
                    std::puts("Ran on the run_loop thread");
                }));

  loop.finish(); // Tell run() to return once the queue drains
}
