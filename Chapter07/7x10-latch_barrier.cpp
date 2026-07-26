// 7x10-latch_barrier.cpp
// ======================
// std::latch as a one-shot barrier, the modern replacement for
// std::promise<void> + std::future<void>::wait() when you only need a signal.
// Requires C++20 (<latch>).

#include <chrono>
#include <iostream>
#include <latch>
#include <syncstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
#define sync_cout std::osyncstream(std::cout)

constexpr int N = 4;

void do_work(int id) {
  sync_cout << "worker " << id << " starting\n";
  std::this_thread::sleep_for(50ms);
  sync_cout << "worker " << id << " done\n";
}

int main() {
  std::latch start{1};

  std::vector<std::jthread> workers;
  for (int i = 0; i < N; ++i) {
    workers.emplace_back([&, i] {
      start.wait(); // park until released
      do_work(i);
    });
  }

  std::this_thread::sleep_for(100ms); // simulate setup
  sync_cout << "releasing all workers\n";
  start.count_down(); // release every waiter at once
}
