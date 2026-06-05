// 8x03-destructor_benchmark.cpp
// Microbenchmark for the destructor-blocks rule. Three patterns spawn the
// same number of threads; only the third lets them overlap.
// Expected on an 8-thread machine with 10 ms tasks: ~334 / ~334 / ~22 ms.

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

constexpr unsigned NUM_TASKS = 32;

unsigned work(unsigned x) {
  std::this_thread::sleep_for(10ms);
  return 2 * x;
}

auto duration_from_ms(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();
}

int main() {
  // Pattern 1: discard the future. Since C++20 this is a [[nodiscard]]
  // warning; we silence it with (void) to keep the build clean, but the
  // behavior is the point: every iteration's temporary future destructor
  // joins before the next iteration starts.
  {
    auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < NUM_TASKS; ++i)
      (void)std::async(std::launch::async, work, i);
    std::cout << "Discarded future:                 "
              << duration_from_ms(t0) << " ms\n";
  }

  // Pattern 2: bind to a per-iteration local. Same end-of-statement
  // destructor; same wall time.
  {
    auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < NUM_TASKS; ++i) {
      auto fut = std::async(std::launch::async, work, i);
      (void)fut;
    }
    std::cout << "Per-iteration auto local:         "
              << duration_from_ms(t0) << " ms\n";
  }

  // Pattern 3: keep every future alive in a vector. Launch loop returns
  // immediately; tasks run in parallel; we drain in a second loop.
  {
    std::vector<std::future<unsigned>> futs;
    futs.reserve(NUM_TASKS);
    auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < NUM_TASKS; ++i)
      futs.push_back(std::async(std::launch::async, work, i));

    std::vector<unsigned> results;
    results.reserve(NUM_TASKS);
    for (auto& f : futs)
      results.push_back(f.get());
    std::cout << "Futures vector, drained at the end: "
              << duration_from_ms(t0) << " ms"
              << " (collected " << results.size() << " results)\n";
  }
}
