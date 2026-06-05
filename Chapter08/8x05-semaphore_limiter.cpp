// 8x05-semaphore_limiter.cpp
// Rate-limiting std::async with std::counting_semaphore (C++20). Caps the
// number of in-flight tasks but does NOT avoid the per-task thread creation
// cost — reaching for this is a signal that we have outgrown std::async.

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <semaphore>
#include <syncstream>
#include <thread>
#include <vector>

#define sync_cout std::osyncstream(std::cout)

using namespace std::chrono_literals;

void do_work(int id) {
  sync_cout << "Running task " << id
            << " on thread " << std::this_thread::get_id() << "\n";
  std::this_thread::sleep_for(200ms);
}

int main() {
  const int total_tasks = 20;
  const unsigned max_in_flight = std::max(
      1u, std::thread::hardware_concurrency());

  std::counting_semaphore<> sem{static_cast<std::ptrdiff_t>(max_in_flight)};

  sync_cout << "Allowing " << max_in_flight
            << " concurrent task(s) for " << total_tasks << " tasks.\n";

  // RAII guard so an exception from do_work never strands a permit.
  struct semaphore_guard {
    std::counting_semaphore<>& sem;
    explicit semaphore_guard(std::counting_semaphore<>& s) : sem(s) { sem.acquire(); }
    ~semaphore_guard() { sem.release(); }
  };

  auto rate_limited = [&sem](int id) {
    semaphore_guard guard{sem};   // blocks when max_in_flight tasks are running
    do_work(id);
  };

  std::vector<std::future<void>> futs;
  futs.reserve(total_tasks);
  for (int i = 0; i < total_tasks; ++i)
    futs.push_back(std::async(std::launch::async, rate_limited, i));

  for (auto& f : futs)
    f.get();

  sync_cout << "All tasks completed.\n";
}
