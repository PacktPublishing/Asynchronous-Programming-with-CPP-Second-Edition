// 3x19-cpp26_std_execution.cpp
// ============================
// PREVIEW: C++26 std::execution (P2300 — senders and receivers).
// Uses the NVIDIA stdexec reference implementation.
//
// Key concepts:
//   scheduler  — lightweight handle to an execution resource (thread pool, GPU, …)
//   sender     — lazy description of async work, not yet started
//   sync_wait  — drives a sender pipeline synchronously and returns the result
//
// Advantages over a hand-built thread pool:
//   - Lazy work description: resources allocated only when execution starts
//   - First-class composition: | then(...) | when_all(...)
//   - Structural cancellation: stop tokens flow through the sender graph
//   - Swappable scheduler: change the execution resource without changing work
//
// Build with: clang++ -std=c++26 -I<path-to-stdexec>/include 3x19-cpp26_std_execution.cpp
// Repository: https://github.com/NVIDIA/stdexec
//
// Requires: stdexec reference implementation (P2300), C++23/26 compiler

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <iostream>

int compute_result() { return 42; }

int main() {
    // Create a fixed-size thread pool and obtain a scheduler handle
    exec::static_thread_pool pool(std::thread::hardware_concurrency());
    auto sched = pool.get_scheduler();

    // Describe the work as a sender pipeline — no execution yet
    auto work = stdexec::schedule(sched)
              | stdexec::then([]{ return compute_result(); });

    // Execute synchronously and retrieve the result
    auto [result] = stdexec::sync_wait(std::move(work)).value();

    std::cout << "result: " << result << "\n";
}
