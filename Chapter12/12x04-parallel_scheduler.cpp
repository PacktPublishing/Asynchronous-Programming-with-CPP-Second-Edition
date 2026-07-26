// 12x04-parallel_scheduler.cpp
// ============================
// The system-wide parallel scheduler: a shared, implementation-managed pool
// obtained (not constructed) via get_parallel_scheduler(). Prefer it for
// general CPU work to avoid oversubscribing the machine with many pools.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//
// NOTE: the parallel scheduler needs its default backend translation unit.
// Build by also compiling stdexec's parallel_scheduler.cpp:
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x04-parallel_scheduler.cpp
//       <path-to-stdexec>/src/parallel_scheduler/parallel_scheduler.cpp
//       -o 12x04-parallel_scheduler

#include <print>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

int work_that_can_run_anywhere() { return 42; }

int main() {
  ex::scheduler auto sched = ex::get_parallel_scheduler();

  auto [value] = ex::sync_wait(
                    ex::schedule(sched)
                  | ex::then([] {
                        return work_that_can_run_anywhere();
                    })).value();

  std::println("parallel scheduler produced {}", value);
}
