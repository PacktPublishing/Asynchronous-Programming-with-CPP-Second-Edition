// 13x07-completion_signatures.cpp
// Every sender advertises, at compile time, how it can complete. The value type
// propagates through each adaptor, which is how sync_wait knows the tuple type
// to return and how the compiler verifies a pipeline is internally consistent.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x07-completion_signatures.cpp
//       -o 13x07-completion_signatures
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <concepts>
#include <exception>
#include <print>
#include <tuple>
#include <type_traits>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
    // The lambda returns double and may throw, so the pipeline can complete with
    // set_value(double) or set_error(exception_ptr).
    ex::sender auto s = ex::just(42) | ex::then([](int x) { return x * 1.5; });

    // The advertised list is a type we can name and assert on. This is the
    // description the compiler propagates through every adaptor.
    static_assert(std::same_as<
        ex::completion_signatures_of_t<decltype(s), ex::env<>>,
        ex::completion_signatures<ex::set_value_t(double),
                                  ex::set_error_t(std::exception_ptr)>>);

    // The value type computed through the pipeline is double: sync_wait returns
    // std::optional<std::tuple<double>>. This is checked entirely at compile time.
    using result_t = std::remove_cvref_t<decltype(ex::sync_wait(s).value())>;
    static_assert(std::same_as<result_t, std::tuple<double>>);

    auto [d] = ex::sync_wait(std::move(s)).value();
    std::println("computed value type is double, value = {}", d);
}

// ---------------------------------------------------------------------------
// What a mismatch looks like. Uncomment to reproduce.
//
//   auto bad = ex::just(42) | ex::then([](std::string s) { return s.size(); });
//
// just(42) completes with int, but the function takes std::string, so no
// completion signatures can be computed and the pipeline is not a sender.
// GCC hides the useful part unless concept diagnostics are deepened:
//
//   g++ -std=c++23 -fconcepts-diagnostics-depth=2 ...
//
// With that flag, stdexec's own diagnostic type appears in the output:
//
//   stdexec::_ERROR_<
//     stdexec::_WHAT_(stdexec::_INVALID_EXPRESSION_),
//     stdexec::_WHY_(stdexec::_FUNCTION_IS_NOT_CALLABLE_WITH_THE_GIVEN_ARGUMENTS_),
//     stdexec::_WHERE_(stdexec::_IN_ALGORITHM_, stdexec::then_t),
//     stdexec::_WITH_FUNCTION_(main()::<lambda(std::string)>),
//     stdexec::_WITH_ARGUMENTS_(int)>
//
// Read it as WHAT, WHY, WHERE: the function is not callable with the given
// arguments, in the then algorithm, with a function taking std::string and an
// argument of type int. Without the flag, only a failed __well_formed_sender
// constraint and a "no match for operator|" error are reported.
