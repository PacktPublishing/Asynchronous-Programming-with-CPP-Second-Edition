// 14x14-asio_scheduler.cpp
// ========================
// A std::execution scheduler over a Boost.Asio executor. schedule() returns a
// sender whose operation state posts to the Asio executor and completes the
// receiver on an I/O thread, honoring the receiver's stop token. Once it
// exists, every sender adaptor from Chapter 13 can target an io_context.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec and Boost 1.86+.
//   g++ -std=c++23 -I<stdexec>/include -I<boost>/include 14x14-asio_scheduler.cpp

#include <boost/asio.hpp>
#include <exception>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexec/execution.hpp>
#include <thread>
#include <utility>

namespace asio = boost::asio;
namespace ex = stdexec;

class asio_scheduler {
   public:
    using scheduler_concept = ex::scheduler_t;

    explicit asio_scheduler(asio::any_io_executor executor) : executor_(std::move(executor)) {}

    // The scheduler is a value: two schedulers over the same executor compare
    // equal, which lets continues_on skip a needless hop.
    bool operator==(const asio_scheduler&) const = default;

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

        // Advertise where we complete, so the framework can reason about it.
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

int main() {
    asio::io_context io;
    auto work = asio::make_work_guard(io);  // keep run() alive
    std::jthread io_thread([&] { io.run(); });

    exec::static_thread_pool cpu{2};

    asio_scheduler io_sched{io.get_executor()};

    auto report = [&] { std::println("on io thread: {}", std::this_thread::get_id() == io_thread.get_id()); };

    // [6] A pipeline that starts on the I/O thread, hops to the CPU pool,
    //     and returns to the I/O thread.
    ex::sender auto pipeline = ex::schedule(io_sched) | ex::then([&] { report(); }) |
                               ex::continues_on(cpu.get_scheduler()) | ex::then([&] {
                                   report();
                                   return 21;
                               }) |
                               ex::continues_on(io_sched) | ex::then([&](int x) {
                                   report();
                                   return x * 2;
                               });

    auto [result] = ex::sync_wait(std::move(pipeline)).value();
    std::println("result = {}", result);

    work.reset();  // let run() return
}
