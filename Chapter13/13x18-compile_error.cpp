// 13x18-compile_error.cpp
// =======================
// Reading a compile error from sender code. just(42) completes with int, but
// the function passed to then takes std::string, so no completion signatures
// can be computed and the pipeline does not compile.
// (Chapter 13, "Completion Signatures and Type Computation".)
//
// The broken pipeline is compiled only when SHOW_COMPILE_ERROR is defined, so
// the default build succeeds. To see the diagnostic the chapter discusses:
//   g++ -std=c++23 -fconcepts-diagnostics-depth=2 -DSHOW_COMPILE_ERROR
//       -I<path-to-stdexec>/include 13x18-compile_error.cpp -o /dev/null
//   clang++ -std=c++23 -DSHOW_COMPILE_ERROR
//       -I<path-to-stdexec>/include 13x18-compile_error.cpp -o /dev/null
// Then search the output for _WHY_ and _WHERE_.
//
// C++26 std::execution (P2300). Requires NVIDIA stdexec.
// Built by default (without the error): CMake fetches stdexec automatically, so
// a plain `cmake --preset debug && cmake --build --preset debug` builds this
// file (see the root CMakeLists.txt and README.md).

#include <print>
#include <stdexec/execution.hpp>
#include <string>

namespace ex = stdexec;

int main() {
#ifdef SHOW_COMPILE_ERROR
    // Does not compile. just(42) completes with int, but the function
    // takes std::string, so no completion signatures can be computed.
    auto bad = ex::just(42) | ex::then([](std::string s) { return s.size(); });
#endif

    // The fix: give the function the type the predecessor produces.
    auto good = ex::just(42) | ex::then([](int x) { return std::to_string(x).size(); });
    auto [digits] = ex::sync_wait(std::move(good)).value();
    std::println("42 has {} digits", digits);
}
