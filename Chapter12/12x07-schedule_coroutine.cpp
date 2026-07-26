// 12x07-schedule_coroutine.cpp
// ============================
// Scheduling a coroutine: co_await schedule(sched) suspends the coroutine and
// resumes it on the scheduler's resource. Everything after the await runs on
// the pool. This is the coroutine spelling of "continue on this scheduler".
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x07-schedule_coroutine.cpp
//       -o 12x07-schedule_coroutine

#include <print>

#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int expensive_computation() { return 42; }

exec::task<int> compute_on(ex::scheduler auto sched) {
  co_await ex::schedule(sched); // Resume on `sched`'s resource

  // From here on, this coroutine runs on the pool
  int result = expensive_computation();
  co_return result;
}

int main() {
  exec::static_thread_pool pool{4};
  ex::scheduler auto sched = pool.get_scheduler();

  auto [result] = ex::sync_wait(compute_on(sched)).value();
  std::println("Coroutine returned {}", result);
}
