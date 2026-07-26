// 12x02-context_lifetime.cpp
// ==========================
// An execution context owns its threads and must outlive the work scheduled
// onto it. sync_wait guarantees the work is done before the pool destructs.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x02-context_lifetime.cpp
//       -o 12x02-context_lifetime

#include <print>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
  {
    exec::static_thread_pool pool{4};
    ex::scheduler auto sched = pool.get_scheduler();

    ex::sync_wait(ex::schedule(sched)
                | ex::then([] {
                    std::println("Running on the pool");
                }));

    // sync_wait has returned, so the work is done...
  } // ...and only now does `pool` destruct, joining its threads.

  std::println("Pool destroyed cleanly");
}
