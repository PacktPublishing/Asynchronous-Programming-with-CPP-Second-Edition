// 13x13-timeout_racing.cpp
// A timeout built by racing: when_any runs the work against a timer, completes
// with whichever finishes first, and requests cancellation of the loser. The
// work polls the ambient stop token, so when the timer wins it stops early --
// structured cancellation doing real work.
//
// PREVIEW: C++26 std::execution (P2300). Requires the NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x13-timeout_racing.cpp
//       -o 13x13-timeout_racing
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <atomic>
#include <chrono>
#include <optional>
#include <print>
#include <thread>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/timed_thread_scheduler.hpp>
#include <exec/when_any.hpp>

using namespace std::chrono_literals;
namespace ex = stdexec;

int main() {
    exec::timed_thread_context   timer_ctx;      // provides schedule_after
    exec::timed_thread_scheduler timer = timer_ctx.get_scheduler();

    exec::static_thread_pool pool{2};
    ex::scheduler auto sched = pool.get_scheduler();

    std::atomic<int> iterations{0};

    // Long-running work that cooperatively checks the ambient stop token.
    ex::sender auto work =
          ex::schedule(sched)
        | ex::let_value([&] {
              return ex::read_env(ex::get_stop_token)
                   | ex::then([&](auto stop_token) {
                         while (!stop_token.stop_requested()) {
                             iterations.fetch_add(1);
                             std::this_thread::sleep_for(1ms);   // a unit of work
                         }
                         return std::optional<int>{42};          // real result
                     });
          });

    // The deadline: completes after 50 ms with an empty optional (timed out).
    ex::sender auto deadline =
        exec::schedule_after(timer, 50ms) | ex::then([] { return std::optional<int>{}; });

    // Race them. when_any completes with the first, and cancels the other.
    auto [result] =
        ex::sync_wait(exec::when_any(std::move(work), std::move(deadline))).value();

    std::println("timed out: {}", !result.has_value());
    std::println("work was cancelled after ~{} iterations", iterations.load());
}
