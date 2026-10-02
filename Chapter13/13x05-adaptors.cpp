// 13x05-adaptors.cpp
// The adaptor algebra: then (map), let_value (flat-map into a dependent
// sender), when_all (fan-in), split (share a result), and stopped_as_optional
// (turn cancellation into an empty optional on the value channel).
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x05-adaptors.cpp
//       -o 13x05-adaptors
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <optional>
#include <print>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

// A dependent asynchronous step: returns a sender, not a value.
ex::sender auto fetch_double(int x) { return ex::just(x * 2.0); }

int main() {
    // then: apply a function on the value channel.
    auto [mapped] = ex::sync_wait(
        ex::just(21) | ex::then([](int x) { return x * 2; })).value();
    std::println("then -> {}", mapped);

    // let_value: splice in a dependent sender.
    auto [dependent] = ex::sync_wait(
        ex::just(10) | ex::let_value([](int id) { return fetch_double(id); })).value();
    std::println("let_value -> {}", dependent);

    // when_all: run several senders and combine their values.
    auto [sum] = ex::sync_wait(
          ex::when_all(ex::just(2), ex::just(3), ex::just(5))
        | ex::then([](int x, int y, int z) { return x + y + z; })).value();
    std::println("when_all -> {}", sum);

    // stopped_as_optional: wrap the value in an optional (which would be empty
    // had the wrapped operation completed on the stopped channel instead).
    auto [maybe] = ex::sync_wait(
        ex::stopped_as_optional(ex::just(1) | ex::then([](int x) { return x; }))).value();
    std::println("stopped_as_optional -> has_value {}", maybe.has_value());

    // into_variant: a sender with more than one value completion cannot be a
    // child of when_all, which needs exactly one per child. into_variant folds
    // them into a single completion carrying a variant of tuples.
    //
    // Two value completions here: set_value(int) on success, and
    // set_value(std::string) if the transformation threw.
    ex::sender auto reading =
          ex::just(21)
        | ex::then([](int v) { return v * 2; })
        | ex::upon_error([](std::exception_ptr) { return std::string{"n/a"}; });

    ex::sender auto joined =
        ex::when_all(ex::into_variant(std::move(reading)), ex::just(1.5))
        | ex::then([](auto result, double scale) {
              // result is std::variant<std::tuple<int>, std::tuple<std::string>>
              std::visit([&](auto& alt) {
                  std::println("into_variant -> reading = {}, scale = {}",
                               std::get<0>(alt), scale);
              }, result);
          });

    ex::sync_wait(std::move(joined));
}
