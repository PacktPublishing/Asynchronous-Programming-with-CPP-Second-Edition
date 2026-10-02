// 13x04-factories.cpp
// Sender factories: just / just_error / just_stopped inject each of the three
// completion channels; upon_error and upon_stopped convert the error and
// stopped channels back to values so sync_wait sees a plain result.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x04-factories.cpp
//       -o 13x04-factories
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>
#include <stdexcept>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
    // just: complete on the value channel with three values.
    auto [a, b, c] = ex::sync_wait(ex::just(1, 2, 3)).value();
    std::println("just -> {} {} {}", a, b, c);

    // just_error: complete on the error channel; recover with upon_error.
    auto [recovered] = ex::sync_wait(
          ex::just_error(std::make_exception_ptr(std::runtime_error{"boom"}))
        | ex::upon_error([](std::exception_ptr) { return -1; })).value();
    std::println("just_error -> recovered {}", recovered);

    // just_stopped: complete on the stopped channel; recover with upon_stopped.
    auto [defaulted] = ex::sync_wait(
          ex::just_stopped()
        | ex::upon_stopped([] { return 0; })).value();
    std::println("just_stopped -> defaulted {}", defaulted);
}
