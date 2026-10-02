// 13x15-retry_backoff.cpp
// Retry with exponential backoff. There is no standard retry adaptor, and a
// recursive sender version fights the type system, so we express the control
// flow as a coroutine: try, and on failure wait a growing delay (via the timed
// scheduler) before trying again, up to a limit. A clear case for reaching for
// a coroutine where the logic is branchy.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x15-retry_backoff.cpp
//       -o 13x15-retry_backoff
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <atomic>
#include <chrono>
#include <print>
#include <stdexcept>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/timed_thread_scheduler.hpp>

namespace ex = stdexec;

// A flaky operation that fails its first two calls, then succeeds.
std::atomic<int> attempts{0};
int flaky_call() {
    int a = ++attempts;
    if (a < 3) {
        throw std::runtime_error("transient failure");
    }
    return a * 10;
}

exec::task<int> with_retry(exec::timed_thread_scheduler timer, int max_retries) {
    for (int i = 0;; ++i) {
        bool failed = false;
        try {
            // A throwing function inside then() surfaces as an error completion,
            // which co_await rethrows here at the suspension point.
            co_return co_await (ex::just() | ex::then([] { return flaky_call(); }));
        } catch (const std::exception&) {
            if (i >= max_retries) {
                throw;                       // give up: propagate the failure
            }
            failed = true;
        }
        // co_await is not allowed inside a catch handler, so back off here.
        if (failed) {
            auto delay = std::chrono::milliseconds(10 * (1 << i));   // 10, 20, 40, ...
            co_await exec::schedule_after(timer, delay);
        }
    }
}

int main() {
    exec::timed_thread_context timer_ctx;

    auto [result] =
        ex::sync_wait(with_retry(timer_ctx.get_scheduler(), 5)).value();

    std::println("succeeded after {} attempts, result = {}", attempts.load(), result);
}
