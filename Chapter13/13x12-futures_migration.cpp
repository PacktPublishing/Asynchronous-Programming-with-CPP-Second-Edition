// 13x12-futures_migration.cpp
// The same computation two ways: the classic eager future model, and the lazy
// sender model. std::async launches immediately and future.get() blocks;
// schedule|then builds a lazy description that runs once at sync_wait.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x12-futures_migration.cpp
//       -o 13x12-futures_migration
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <future>
#include <print>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

int main() {
    // Future model: eager launch, blocking get.
    std::future<int> fut = std::async(std::launch::async, [] { return 20 + 22; });
    std::println("future.get()  = {}", fut.get());

    // Sender model: lazy description, run once at the edge.
    exec::static_thread_pool pool{2};
    ex::sender auto work =
        ex::schedule(pool.get_scheduler()) | ex::then([] { return 20 + 22; });
    auto [n] = ex::sync_wait(std::move(work)).value();
    std::println("sync_wait     = {}", n);
}
