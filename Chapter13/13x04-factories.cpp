// 13x04-factories.cpp
// ===================
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
    // A factory makes a sender from values. Nothing runs yet.
    ex::sender auto s = ex::just(42);       // completes with set_value(42)
    ex::sender auto p = ex::just(1, 2, 3);  // completes with set_value(1, 2, 3)
    auto [forty_two] = ex::sync_wait(std::move(s)).value();
    auto [x, y, z] = ex::sync_wait(std::move(p)).value();
    std::println("just -> {} and {} {} {}", forty_two, x, y, z);

    // read_env: turn an environment query into a value. Under sync_wait the
    // environment's scheduler is the run_loop that sync_wait drives, so we use
    // it inside the same pipeline: that run_loop ends when sync_wait returns.
    ex::sender auto here = ex::read_env(ex::get_scheduler);  // yields the current scheduler
    auto [ran] = ex::sync_wait(std::move(here) | ex::let_value([](auto sched) {
                                   return ex::schedule(sched) | ex::then([] { return true; });
                               }))
                     .value();
    std::println("read_env(get_scheduler) -> a scheduler we can use: {}", ran);

    // just: complete on the value channel with three values.
    auto [a, b, c] = ex::sync_wait(ex::just(1, 2, 3)).value();
    std::println("just -> {} {} {}", a, b, c);

    // just_error: complete on the error channel; recover with upon_error.
    auto [recovered] = ex::sync_wait(ex::just_error(std::make_exception_ptr(std::runtime_error{"boom"})) |
                                     ex::upon_error([](std::exception_ptr) { return -1; }))
                           .value();
    std::println("just_error -> recovered {}", recovered);

    // just_stopped: complete on the stopped channel; recover with upon_stopped.
    auto [defaulted] = ex::sync_wait(ex::just_stopped() | ex::upon_stopped([] { return 0; })).value();
    std::println("just_stopped -> defaulted {}", defaulted);
}
