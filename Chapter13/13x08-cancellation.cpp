// 13x08-cancellation.cpp
// Cancellation is a first-class completion channel. A leaf can read the ambient
// stop token from its environment and poll it cooperatively; when_all uses the
// stopped channel to tear down siblings when one branch stops.
//
// Under sync_wait the installed token is a never-stop token, so the cooperative
// loop below runs to completion. A real stop token is installed by a consumer
// such as an async scope; see 13x06-async_scope.cpp for owned work.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x08-cancellation.cpp
//       -o 13x08-cancellation
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <print>
#include <string>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

int main() {
    // A cancellable leaf: poll the ambient stop token between chunks of work.
    ex::sender auto cancellable =
          ex::read_env(ex::get_stop_token)
        | ex::then([](auto stop_token) {
              int chunks = 0;
              while (!stop_token.stop_requested() && chunks < 5) {
                  ++chunks;   // do one chunk of work
              }
              return chunks;
          });

    auto [chunks] = ex::sync_wait(std::move(cancellable)).value();
    std::println("completed {} chunks (token never fired)", chunks);

    // A stopped completion propagates like a value or an error, and upon_stopped
    // intercepts it on the stopped channel.
    auto [status] = ex::sync_wait(
          ex::just_stopped()
        | ex::upon_stopped([] { return std::string{"cancelled"}; })).value();
    std::println("stopped channel -> {}", status);
}
