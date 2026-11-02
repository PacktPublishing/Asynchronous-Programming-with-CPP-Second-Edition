// asio_bridge.hpp
// ===============
// The bridge between Boost.Asio and C++26 std::execution used by the Chapter 14
// examples 14x14 to 14x17:
//
//   asio_scheduler  -- a std::execution scheduler over an Asio executor, so a
//                      sender pipeline can start on, or continue on, an
//                      io_context.
//   use_sender      -- an Asio completion token that turns any asynchronous
//                      operation into a sender. The receiver's stop token is
//                      forwarded to the operation's cancellation slot, and the
//                      completion handler's arguments become the three
//                      completion channels.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec and
// Boost 1.86+.

#pragma once

#include <boost/asio.hpp>
#include <exception>
#include <memory>
#include <optional>
#include <stdexec/execution.hpp>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace asio = boost::asio;
namespace ex = stdexec;

// ---------------------------------------------------------------------------
// asio_scheduler
// ---------------------------------------------------------------------------

class asio_scheduler {
   public:
    using scheduler_concept = ex::scheduler_t;

    explicit asio_scheduler(asio::any_io_executor executor) : executor_(std::move(executor)) {}

    // Two schedulers over the same executor compare equal, which lets
    // continues_on skip a needless hop.
    bool operator==(const asio_scheduler &) const = default;

    asio::any_io_executor get_executor() const noexcept { return executor_; }

    // --- The operation state: start() posts to the executor ---
    template <class Receiver>
    struct schedule_operation {
        using operation_state_concept = ex::operation_state_t;

        asio::any_io_executor executor;
        Receiver rcvr;

        void start() & noexcept {
            asio::post(executor, [this] {  // [1]
                auto token = ex::get_stop_token(ex::get_env(rcvr));
                if (token.stop_requested()) {  // [2]
                    ex::set_stopped(std::move(rcvr));
                } else {
                    ex::set_value(std::move(rcvr));  // [3]
                }
            });
        }
    };

    // --- The sender returned by schedule() ---
    struct sender {
        using sender_concept = ex::sender_t;
        using completion_signatures = ex::completion_signatures<  // [4]
            ex::set_value_t(), ex::set_stopped_t()>;

        asio::any_io_executor executor;

        template <class Receiver>
        schedule_operation<Receiver> connect(Receiver rcvr) const {
            return {executor, std::move(rcvr)};
        }

        // Advertise where we complete so the framework can reason about it.
        struct env {
            asio::any_io_executor executor;
            asio_scheduler query(ex::get_completion_scheduler_t<ex::set_value_t>) const noexcept {
                return asio_scheduler{executor};
            }
        };
        env get_env() const noexcept { return {executor}; }  // [5]
    };

    sender schedule() const noexcept { return {executor_}; }

   private:
    asio::any_io_executor executor_;
};

// ---------------------------------------------------------------------------
// use_sender
// ---------------------------------------------------------------------------

struct use_sender_t {};
inline constexpr use_sender_t use_sender;

namespace bridge_detail {

// Split an Asio completion signature into "does it start with an error_code?"
// and "the values that follow it".
template <class... Args>
struct signature_traits {
    static constexpr bool has_error = false;
    using values = std::tuple<std::decay_t<Args>...>;
};

template <class... Args>
struct signature_traits<boost::system::error_code, Args...> {
    static constexpr bool has_error = true;
    using values = std::tuple<std::decay_t<Args>...>;
};

template <class Tuple>
struct value_signature;

template <class... Values>
struct value_signature<std::tuple<Values...>> {
    using type = ex::set_value_t(Values...);
};

// The operation state: owns the deferred Asio operation, the receiver, the
// cancellation signal that the stop token is bridged to, and the stop callback.
template <class Deferred, class Receiver, class... Args>
struct use_sender_operation {
    using operation_state_concept = ex::operation_state_t;
    using traits = signature_traits<Args...>;
    using stop_token = ex::stop_token_of_t<ex::env_of_t<Receiver>>;
    struct on_stop {
        use_sender_operation *self;
        void operator()() const noexcept { self->request_cancel(); }
    };
    using stop_callback = ex::stop_callback_for_t<stop_token, on_stop>;

    Deferred op;
    Receiver rcvr;
    asio::any_io_executor executor;
    std::shared_ptr<asio::cancellation_signal> signal;
    std::optional<stop_callback> callback;

    use_sender_operation(Deferred op, Receiver rcvr, asio::any_io_executor executor)
        : op(std::move(op)),
          rcvr(std::move(rcvr)),
          executor(std::move(executor)),
          signal(std::make_shared<asio::cancellation_signal>()) {}

    void start() & noexcept {
        auto token = ex::get_stop_token(ex::get_env(rcvr));
        if (token.stop_requested()) {  // [1]
            ex::set_stopped(std::move(rcvr));
            return;
        }
        callback.emplace(token, on_stop{this});      // [2]
        std::move(op)(asio::bind_cancellation_slot(  // [3]
            signal->slot(), [this](Args... args) { complete(std::move(args)...); }));
        if (token.stop_requested()) request_cancel();  // [4]
    }

    // Cancellation must be delivered on the operation's executor, never
    // concurrently with its completion, so we dispatch the emit there.
    void request_cancel() noexcept {
        asio::dispatch(executor, [signal = signal] {  // [5]
            signal->emit(asio::cancellation_type::all);
        });
    }

    void complete(Args... args) noexcept {
        callback.reset();  // [6]
        if constexpr (traits::has_error) {
            deliver(std::move(args)...);
        } else {
            ex::set_value(std::move(rcvr), std::move(args)...);
        }
    }

    template <class... Rest>
    void deliver(boost::system::error_code ec, Rest... rest) noexcept {
        auto token = ex::get_stop_token(ex::get_env(rcvr));
        if (ec == asio::error::operation_aborted && token.stop_requested()) {
            ex::set_stopped(std::move(rcvr));  // [7]
        } else if (ec) {
            ex::set_error(std::move(rcvr), std::make_exception_ptr(  // [8]
                                               boost::system::system_error(ec)));
        } else {
            ex::set_value(std::move(rcvr), std::move(rest)...);  // [9]
        }
    }
};

template <class Deferred, class... Args>
struct sender {
    using sender_concept = ex::sender_t;
    using traits = signature_traits<Args...>;

    using completion_signatures = ex::completion_signatures<typename value_signature<typename traits::values>::type,
                                                            ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>;

    Deferred op;
    asio::any_io_executor executor;

    template <class Receiver>
    use_sender_operation<Deferred, Receiver, Args...> connect(Receiver rcvr) && {
        return {std::move(op), std::move(rcvr), executor};
    }
};

// Most Asio initiation objects carry the executor their operation completes on.
template <class Initiation>
asio::any_io_executor executor_of(const Initiation &init) {
    if constexpr (requires { init.get_executor(); }) {
        return init.get_executor();
    } else {
        return asio::system_executor{};
    }
}

}  // namespace bridge_detail

namespace boost::asio {

// The token's async_result: build the deferred operation Asio already knows how
// to make, then wrap it in a sender.
template <class R, class... Args>
class async_result<use_sender_t, R(Args...)> {
   public:
    template <class Initiation, class... InitArgs>
    static auto initiate(Initiation &&init, use_sender_t, InitArgs &&...args) {
        auto executor = bridge_detail::executor_of(init);
        auto op = async_result<deferred_t, R(Args...)>::initiate(std::forward<Initiation>(init), deferred,
                                                                 std::forward<InitArgs>(args)...);
        return bridge_detail::sender<decltype(op), Args...>{std::move(op), std::move(executor)};
    }
};

}  // namespace boost::asio
