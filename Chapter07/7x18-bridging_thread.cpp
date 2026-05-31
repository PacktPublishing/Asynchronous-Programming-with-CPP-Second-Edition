// 7x18-bridging_thread.cpp
// Bridge a legacy std::future-returning API into a sender pipeline by lifting
// the get() call onto a thread-pool scheduler. Burns one OS thread per pending
// bridge call, but is the simplest correct option.
//
// Requires NVIDIA stdexec. Excluded from the default CMake build.
//   clang++ -std=c++26 -I<path-to-stdexec>/include 7x18-bridging_thread.cpp

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

using namespace std::chrono_literals;
namespace ex = stdexec;

// Stand-in for any third-party API that returns std::future<T>.
std::future<int> legacy_compute() {
  return std::async(std::launch::async, []{
    std::this_thread::sleep_for(300ms);
    return 42;
  });
}

int main() {
  exec::static_thread_pool pool(2);
  auto sched = pool.get_scheduler();

  auto legacy = legacy_compute();

  // The bridge: schedule a worker, then call .get() inside it. The worker
  // blocks for as long as the legacy future takes to resolve.
  auto bridge = ex::schedule(sched)
              | ex::then([fut = std::move(legacy)]() mutable {
                  return fut.get();
                });

  auto [result] = ex::sync_wait(std::move(bridge)).value();
  std::cout << "bridged result: " << result << "\n";
}
