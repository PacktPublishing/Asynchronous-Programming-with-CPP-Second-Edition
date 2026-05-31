// 7x17-senders_dag.cpp
// PREVIEW: C++26 std::execution (P2300) refactor of the asynchronous task DAG
// from 7x15-task_dag.cpp. Compare line counts.
//
// Requires NVIDIA stdexec reference implementation (P2300 is C++26 but no
// shipping standard library implements it yet).
//   Repository: https://github.com/NVIDIA/stdexec
//
// Build:
//   clang++ -std=c++26 -I<path-to-stdexec>/include 7x17-senders_dag.cpp
//
// This file is excluded from the default CMake build (see CMakeLists.txt).

#include <chrono>
#include <iostream>
#include <syncstream>
#include <thread>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

using namespace std::chrono_literals;
namespace ex = stdexec;
#define sync_cout std::osyncstream(std::cout)

int main() {
  exec::static_thread_pool pool(8);
  auto sched = pool.get_scheduler();

  auto sleep1s = []{ std::this_thread::sleep_for(1s); };
  auto sleep2s = []{ std::this_thread::sleep_for(2s); };

  // Build the DAG with split() so task2's completion can fan out to two
  // downstream operations (task3 and task4) without re-running task2.
  auto task1 = ex::schedule(sched) | ex::then(sleep1s);
  auto task2 = ex::starts_on(sched, std::move(task1)) | ex::then(sleep2s) | ex::split();
  auto task3 = ex::starts_on(sched, task2) | ex::then(sleep1s);
  auto task4 = ex::starts_on(sched, task2) | ex::then(sleep2s);
  auto task5 = ex::when_all(std::move(task3), std::move(task4))
             | ex::then([]{ std::this_thread::sleep_for(2s); });

  sync_cout << "DAG starting\n";
  ex::sync_wait(std::move(task5));
  sync_cout << "All done!\n";
}
