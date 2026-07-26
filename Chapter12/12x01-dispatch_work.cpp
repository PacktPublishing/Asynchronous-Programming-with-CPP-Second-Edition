// 12x01-dispatch_work.cpp
// =======================
// A first look at the execution framework.
// Run work on a thread pool and block for the result.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x01-dispatch_work.cpp
//       -o 12x01-dispatch_work

#include <print>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
  exec::static_thread_pool pool{4};                       // [1] An execution context
  ex::scheduler auto sched = pool.get_scheduler();        // [2] A handle onto it

  ex::sender auto work = ex::schedule(sched)              // [3] Start on the pool
                         | ex::then([] { return 21; })    // [4] Run a function there
                         | ex::then([](int x) { return x * 2; });

  auto [result] = ex::sync_wait(std::move(work)).value(); // [5] Block for it
  std::println("result = {}", result);                    // Prints: result = 42
}
