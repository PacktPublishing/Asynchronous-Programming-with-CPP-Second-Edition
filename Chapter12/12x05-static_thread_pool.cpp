// 12x05-static_thread_pool.cpp
// ============================
// static_thread_pool is a fixed-size pool we own: workers start on construction
// and join on destruction, and its scheduler distributes work across them.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x05-static_thread_pool.cpp
//       -o 12x05-static_thread_pool
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>
#include <thread>

namespace ex = stdexec;

int main() {
    exec::static_thread_pool pool{std::thread::hardware_concurrency()};
    ex::scheduler auto sched = pool.get_scheduler();

    auto [sum] =
        ex::sync_wait(ex::schedule(sched) | ex::then([] { return 40; }) | ex::then([](int x) { return x + 2; }))
            .value();

    std::println("static_thread_pool produced {}", sum);
}
