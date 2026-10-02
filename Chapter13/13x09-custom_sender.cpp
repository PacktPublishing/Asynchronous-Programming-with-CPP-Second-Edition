// 13x09-custom_sender.cpp
// A custom sender in three parts: the sender (declares its completion
// signatures and knows how to connect), the operation state (whose start()
// does the work and completes the receiver), and a factory function.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x09-custom_sender.cpp
//       -o 13x09-custom_sender
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>
#include <type_traits>
#include <utility>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

template <class F, class Receiver>
struct value_operation {
    F        func;
    Receiver rcvr;

    void start() & noexcept {
        try {
            ex::set_value(std::move(rcvr), func());        // [1] success channel
        } catch (...) {
            ex::set_error(std::move(rcvr),                 // [2] error channel
                          std::current_exception());
        }
    }
};

template <class F>
struct value_sender {
    using sender_concept = ex::sender_t;

    using completion_signatures = ex::completion_signatures<   // [3]
        ex::set_value_t(std::invoke_result_t<F&>),
        ex::set_error_t(std::exception_ptr)>;

    F func;

    template <class Receiver>
    value_operation<F, Receiver> connect(Receiver rcvr) && {   // [4]
        return {std::move(func), std::move(rcvr)};
    }
};

template <class F>
value_sender<F> run_and_value(F f) { return {std::move(f)}; }

int main() {
    auto [n] = ex::sync_wait(
        run_and_value([] { return 21; }) | ex::then([](int x) { return x * 2; })
    ).value();

    std::println("custom sender produced {}", n);   // 42
}
