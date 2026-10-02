// 13x03-receiver_and_env.cpp
// The receiver contract in full. A receiver is any object completed through the
// three customization points that also exposes an environment. The environment
// carries the ambient queries (stop token, scheduler) that senders read with
// get_env(rcvr). read_env then lifts such a query onto the value channel so a
// pipeline can act on the answer.
//
// The chapter shows the minimal receiver and the environment-carrying one as
// two separate illustrations, so they are kept in namespaces here to let both
// live in one translation unit under the same name.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x03-receiver_and_env.cpp
//       -o 13x03-receiver_and_env
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

// A minimal receiver: the three completion signals and nothing else.
namespace contract {

struct my_receiver {
    using receiver_concept = ex::receiver_t;

    void set_value(int value) && noexcept {
        std::println("got {}", value);
    }
    void set_error(std::exception_ptr) && noexcept {
        std::println("failed");
    }
    void set_stopped() && noexcept {
        std::println("stopped");
    }
};

}   // namespace contract

// A receiver that also answers ambient queries through its environment.
namespace with_env {

struct my_env {
    ex::inplace_stop_token token;
    exec::static_thread_pool::scheduler sched;

    // One overload per query this environment answers.
    auto query(ex::get_stop_token_t) const noexcept { return token; }
    auto query(ex::get_scheduler_t)  const noexcept { return sched; }
};

struct my_receiver {
    using receiver_concept = ex::receiver_t;
    my_env env;

    // Expose the environment so senders can query it.
    my_env get_env() const noexcept { return env; }

    void set_value() && noexcept { std::println("env receiver: set_value"); }
    void set_error(std::exception_ptr) && noexcept { std::println("env receiver: set_error"); }
    void set_stopped() && noexcept { std::println("env receiver: set_stopped"); }
};

}   // namespace with_env

int main() {
    // 1. The contract, exercised by hand: connect a sender to a receiver and
    //    start it. just(7) completes with set_value(7).
    auto op = ex::connect(ex::just(7), contract::my_receiver{});
    ex::start(op);

    // 2. The environment reaches the sender. read_env(get_scheduler) is
    //    answered by my_env::query, not by the consumer, because this receiver
    //    is the one the pipeline was connected to.
    exec::static_thread_pool pool{2};
    ex::inplace_stop_source source;
    with_env::my_env env{source.get_token(), pool.get_scheduler()};

    auto env_op = ex::connect(
          ex::read_env(ex::get_scheduler)
        | ex::then([](ex::scheduler auto) { std::println("read the ambient scheduler"); }),
        with_env::my_receiver{env});
    ex::start(env_op);

    // 3. The same query inside a consumed pipeline: sync_wait installs its own
    //    environment, which answers get_stop_token with a never_stop_token.
    ex::sender auto reads_token =
          ex::read_env(ex::get_stop_token)
        | ex::then([](auto token) { return token.stop_requested(); });

    auto [requested] = ex::sync_wait(std::move(reads_token)).value();
    std::println("stop requested at start? {}", requested);

    // 4. Reading the ambient scheduler and scheduling follow-up work onto it.
    ex::sender auto reads_sched =
          ex::starts_on(pool.get_scheduler(), ex::read_env(ex::get_scheduler))
        | ex::let_value([](ex::scheduler auto sc) {
              return ex::schedule(sc) | ex::then([] { return 7; });
          });

    auto [n] = ex::sync_wait(std::move(reads_sched)).value();
    std::println("work on ambient scheduler returned {}", n);
}
