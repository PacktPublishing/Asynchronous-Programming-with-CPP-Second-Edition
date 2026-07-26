// 12x08-bulk.cpp
// ==============
// bulk(sndr, n, f) invokes f(i, values...) for each index in [0, n).
// A work-stealing scheduler (static_thread_pool) spreads the iterations
// across its workers automatically.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x08-bulk.cpp
//       -o 12x08-bulk

#include <cstddef>
#include <numeric>
#include <print>
#include <thread>
#include <vector>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int transform(int x) { return x * 2; }

int main() {
  constexpr std::size_t N = 1'000'000;
  std::vector<int> input(N), output(N);
  std::iota(input.begin(), input.end(), 0);

  exec::static_thread_pool pool{std::thread::hardware_concurrency()};
  ex::scheduler auto sched = pool.get_scheduler();

  ex::sync_wait(ex::schedule(sched)
              | ex::bulk(ex::par, N, [&](std::size_t i) {
                  output[i] = transform(input[i]);    // Runs across all workers
                }));

  std::println("output[7] = {}, output[{}] = {}",
               output[7], N - 1, output[N - 1]);
}
