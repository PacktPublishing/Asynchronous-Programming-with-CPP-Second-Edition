// 13x14-parallel_map_reduce.cpp
// A parallel map-reduce as one sender pipeline: bulk maps a function over every
// element across the pool's workers, then a final then() reduces the results.
// The GPU section retargets this exact shape onto nvexec::stream_scheduler
// (see 13x16-gpu_map_reduce.cpp) -- write the algorithm once, choose where it
// runs.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x14-parallel_map_reduce.cpp
//       -o 13x14-parallel_map_reduce
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <cstddef>
#include <numeric>
#include <print>
#include <thread>
#include <vector>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

int main() {
    constexpr std::size_t N = 1'000'000;
    std::vector<double> input(N), squared(N);
    std::iota(input.begin(), input.end(), 1.0);   // 1, 2, 3, ...

    exec::static_thread_pool pool{std::thread::hardware_concurrency()};
    ex::scheduler auto sched = pool.get_scheduler();

    ex::sender auto pipeline =
          ex::schedule(sched)
        | ex::bulk(ex::par, N, [&](std::size_t i) {          // map, in parallel
              squared[i] = input[i] * input[i];
          })
        | ex::then([&] {                                     // reduce
              return std::reduce(squared.begin(), squared.end(), 0.0);
          });

    auto [sum] = ex::sync_wait(std::move(pipeline)).value();
    std::println("sum of the first {} squares = {}", N, sum);
}
