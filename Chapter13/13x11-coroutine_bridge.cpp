// 13x11-coroutine_bridge.cpp
// Senders and coroutines are interchangeable. Inside an exec::task a sender is
// awaitable: co_await suspends, the sender runs, and its value becomes the
// result of the co_await. exec::task is itself a sender, so it composes with
// the pipe algebra.
//
// The custom sender from 13x09 is repeated here to show that it needs no extra
// work to be awaitable: satisfying the sender concept is the whole requirement,
// so co_await treats value_sender exactly as it treats ex::just.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x11-coroutine_bridge.cpp
//       -o 13x11-coroutine_bridge
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>
#include <type_traits>
#include <utility>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>

namespace ex = stdexec;

// The custom sender of 13x09, unchanged.
template <class F, class Receiver>
struct value_operation {
    F        func;
    Receiver rcvr;

    void start() & noexcept {
        try {
            ex::set_value(std::move(rcvr), func());
        } catch (...) {
            ex::set_error(std::move(rcvr), std::current_exception());
        }
    }
};

template <class F>
struct value_sender {
    using sender_concept = ex::sender_t;

    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(std::invoke_result_t<F&>),
        ex::set_error_t(std::exception_ptr)>;

    F func;

    template <class Receiver>
    value_operation<F, Receiver> connect(Receiver rcvr) && {
        return {std::move(func), std::move(rcvr)};
    }
};

template <class F>
value_sender<F> run_and_value(F f) { return {std::move(f)}; }

exec::task<int> pipeline_in_a_coroutine(ex::scheduler auto sched) {
    co_await ex::schedule(sched);         // hop onto the pool
    int a = co_await run_and_value([] {   // await our custom sender
        return 20;
    });
    int b = co_await ex::just(22);        // await a factory
    co_return a + b;                      // == 42
}

int main() {
    exec::static_thread_pool pool{4};
    ex::scheduler auto sched = pool.get_scheduler();

    // exec::task is a sender, so it drops straight into sync_wait or a chain.
    auto [result] = ex::sync_wait(pipeline_in_a_coroutine(sched)).value();
    std::println("coroutine pipeline returned {}", result);
}
