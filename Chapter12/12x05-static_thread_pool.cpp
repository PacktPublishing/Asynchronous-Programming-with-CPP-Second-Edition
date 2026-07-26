// 12x05-static_thread_pool.cpp
// ============================
// static_thread_pool is a fixed-size pool we own.
// Workers start on construction and join on destruction,
// and its scheduler distributes work across them.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x05-static_thread_pool.cpp
//       -o 12x05-static_thread_pool

#include <print>
#include <thread>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
  exec::static_thread_pool pool{std::thread::hardware_concurrency()};

  ex::scheduler auto sched = pool.get_scheduler();

  auto [sum] = ex::sync_wait(ex::schedule(sched)
                           | ex::then([] { return 40; }) |
                             ex::then([](int x) { return x + 2; }))
                            .value();

  std::println("static_thread_pool produced {}", sum);
}
