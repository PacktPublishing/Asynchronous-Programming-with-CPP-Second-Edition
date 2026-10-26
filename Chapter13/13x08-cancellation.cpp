// 13x08-cancellation.cpp
// ======================
// Cancellation is a first-class completion channel. A leaf can read the ambient
// stop token from its environment and poll it cooperatively; when_all uses the
// stopped channel to tear down siblings when one branch stops.
//
// Under sync_wait the installed token is a never-stop token, so the cooperative
// loop below runs to completion. A real stop token can be injected with
// write_env, as the second run shows, or installed by a consumer such as a
// counting_scope or by when_all and when_any for their children.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x08-cancellation.cpp
//       -o 13x08-cancellation
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <atomic>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>
#include <string>

namespace ex = stdexec;

// One unit of work. Returns false when there is nothing left to do.
std::atomic<int> chunks_done{0};
bool do_one_chunk() { return ++chunks_done < 5; }

int main() {
    exec::static_thread_pool pool{2};
    ex::scheduler auto sched = pool.get_scheduler();

    // A cancellable leaf: poll the ambient stop token between chunks of work.
    ex::sender auto cancellable = ex::schedule(sched) | ex::let_value([] {
                                      return ex::read_env(ex::get_stop_token) | ex::then([](auto stop_token) {
                                                 while (not stop_token.stop_requested()) {
                                                     // cooperative check
                                                     if (not do_one_chunk()) break;
                                                 }
                                             });
                                  });

    // Under sync_wait the token is a never_stop_token: the loop runs to the end.
    ex::sync_wait(std::move(cancellable));
    std::println("completed {} chunks (token never fired)", chunks_done.load());

    // The same leaf with a stop already requested, injected with write_env:
    // the loop checks the token first and does no work.
    chunks_done = 0;
    ex::inplace_stop_source source;
    source.request_stop();
    ex::sync_wait(ex::write_env(ex::read_env(ex::get_stop_token) | ex::then([](auto stop_token) {
                                    while (not stop_token.stop_requested()) {
                                        if (not do_one_chunk()) break;
                                    }
                                }),
                                ex::prop{ex::get_stop_token, source.get_token()}));
    std::println("completed {} chunks (stop requested first)", chunks_done.load());

    // A stopped completion propagates like a value or an error, and upon_stopped
    // intercepts it on the stopped channel.
    auto [status] =
        ex::sync_wait(ex::just_stopped() | ex::upon_stopped([] { return std::string{"cancelled"}; })).value();
    std::println("stopped channel -> {}", status);
}
