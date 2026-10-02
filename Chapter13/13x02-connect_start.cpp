// 13x02-connect_start.cpp
// The lazy lifecycle, unrolled by hand: connect() joins a sender to a receiver
// and returns an operation state; start() launches it. This is what consumers
// like sync_wait do for us.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x02-connect_start.cpp
//       -o 13x02-connect_start
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>
#include <utility>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

struct printing_receiver {
    using receiver_concept = ex::receiver_t;

    void set_value(int v) && noexcept { std::println("set_value({})", v); }
    void set_error(std::exception_ptr) && noexcept { std::println("set_error"); }
    void set_stopped() && noexcept { std::println("set_stopped"); }
};

int main() {
    ex::sender auto s = ex::just(42) | ex::then([](int x) { return x + 1; });

    auto op = ex::connect(std::move(s), printing_receiver{});  // build the operation
    ex::start(op);                                             // run it
    // printing_receiver::set_value(43) is called
}
