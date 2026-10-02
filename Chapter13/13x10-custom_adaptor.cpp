// 13x10-custom_adaptor.cpp
// A custom adaptor is a sender that wraps another sender, connecting it to a
// receiver of its own making that intercepts the completion signals. This
// minimal then-like adaptor transforms the value channel and forwards error,
// stopped, and the environment untouched.
//
// The operation state is the part worth studying: it holds the wrapped
// sender's operation state as a member, so connecting a pipeline of any
// length produces one nested object with a single lifetime.
//
// It is deliberately monomorphic (it reports set_value(int)) to keep the
// completion-signature plumbing readable. That shortcut is not free. Pass a
// function returning double and this still compiles, but the result is
// truncated to int with no diagnostic, because the signatures a sender
// declares are simply believed. A generic adaptor computes its signatures from
// the wrapped sender instead. P2300 specifies transform_completion_signatures_of
// for that, although stdexec currently exposes it only as an internal detail.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++23 -I<path-to-stdexec>/include 13x10-custom_adaptor.cpp
//       -o 13x10-custom_adaptor
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <exception>
#include <print>
#include <utility>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

// Intercept the value channel; forward the other two channels and the env.
template <class F, class Receiver>
struct then_receiver {
    using receiver_concept = ex::receiver_t;
    F        func;
    Receiver rcvr;

    template <class... Values>
    void set_value(Values&&... vs) && noexcept {
        try {
            ex::set_value(std::move(rcvr), func(std::forward<Values>(vs)...));
        } catch (...) {
            ex::set_error(std::move(rcvr), std::current_exception());
        }
    }
    template <class E>
    void set_error(E&& e) && noexcept {
        ex::set_error(std::move(rcvr), std::forward<E>(e));
    }
    void set_stopped() && noexcept { ex::set_stopped(std::move(rcvr)); }

    decltype(auto) get_env() const noexcept { return ex::get_env(rcvr); }
};

// The operation state owns the wrapped sender's operation state.
template <class Sender, class F, class Receiver>
struct then_operation {
    ex::connect_result_t<Sender, then_receiver<F, Receiver>> inner_op;

    then_operation(Sender&& sndr, F f, Receiver rcvr)
        : inner_op(ex::connect(std::move(sndr),
                               then_receiver<F, Receiver>{std::move(f),
                                                          std::move(rcvr)})) {}

    void start() & noexcept { ex::start(inner_op); }
};

template <class Sender, class F>
struct then_sender {
    using sender_concept = ex::sender_t;
    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(int),
        ex::set_error_t(std::exception_ptr),
        ex::set_stopped_t()>;

    Sender sndr;
    F      func;

    template <class Receiver>
    then_operation<Sender, F, Receiver> connect(Receiver rcvr) && {
        return {std::move(sndr), std::move(func), std::move(rcvr)};
    }
};

template <class Sender, class F>
then_sender<Sender, F> my_then(Sender s, F f) {
    return {std::move(s), std::move(f)};
}

int main() {
    auto [n] = ex::sync_wait(
        my_then(ex::just(21), [](int x) { return x * 2; })
    ).value();
    std::println("custom adaptor produced {}", n);   // 42
}
