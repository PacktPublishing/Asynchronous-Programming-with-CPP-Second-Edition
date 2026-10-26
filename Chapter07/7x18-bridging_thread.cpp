// 7x18-bridging_thread.cpp
// ========================
// Bridge a legacy std::future-returning API into a sender pipeline by lifting
// the get() call onto a thread-pool scheduler. Burns one OS thread per pending
// bridge call, but is the simplest correct option.
//
// Requires NVIDIA stdexec, which the root CMakeLists.txt fetches, so this is
// built by default (see CMakeLists.txt).
//   clang++ -std=c++23 -I<path-to-stdexec>/include 7x18-bridging_thread.cpp

#include <chrono>
#include <exec/static_thread_pool.hpp>
#include <future>
#include <iostream>
#include <stdexec/execution.hpp>
#include <thread>

using namespace std::chrono_literals;
namespace ex = stdexec;

// Stand-in for any third-party API that returns std::future<T>.
std::future<int> legacy_compute() {
    return std::async(std::launch::async, [] {
        std::this_thread::sleep_for(300ms);
        return 42;
    });
}

int main() {
    exec::static_thread_pool pool(2);

    auto legacy_future = legacy_compute();

    // The bridge: schedule a worker, then call .get() inside it. The worker
    // blocks for as long as the legacy future takes to resolve. The future
    // outlives the pipeline, because sync_wait below waits for it.
    auto bridge =
        ex::schedule(pool.get_scheduler()) | ex::then([&] { return legacy_future.get(); });  // blocks the worker

    auto [result] = ex::sync_wait(std::move(bridge)).value();
    std::cout << "bridged result: " << result << "\n";
}
