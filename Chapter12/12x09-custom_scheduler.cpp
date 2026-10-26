// 12x09-custom_scheduler.cpp
// ==========================
// Building a custom execution context and scheduler from scratch: a single
// background thread draining a queue, a scheduler handle, and the sender /
// operation-state pair its schedule() returns. Demonstrates the three pieces
// every scheduler owes the framework.
//
// PREVIEW: C++26 std::execution (P2300). Requires NVIDIA stdexec.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x09-custom_scheduler.cpp
//       -o 12x09-custom_scheduler
// Built by default: CMake fetches stdexec automatically, so a plain
// `cmake --preset debug && cmake --build --preset debug` builds this file
// (see the root CMakeLists.txt and README.md).

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexec/execution.hpp>
#include <thread>
#include <utility>

namespace ex = stdexec;

// --- The context: owns a worker thread and a task queue ---
class manual_scheduler;  // the handle, defined after the sender below

class manual_context {
   public:
    manual_context() : worker_([this] { run(); }) {}

    ~manual_context() {
        {
            std::lock_guard lk{mtx_};
            stop_ = true;
        }
        cv_.notify_one();
        worker_.join();  // drain then join
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard lk{mtx_};
            queue_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    manual_scheduler get_scheduler() noexcept;  // returns manual_scheduler{this}

   private:
    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lk{mtx_};
                cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            task();  // run off the lock
        }
    }

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> queue_;
    bool stop_ = false;
    std::jthread worker_;
};

// --- The operation state: start() enqueues work that completes the receiver ---
template <class Receiver>
struct manual_operation {
    manual_context* ctx;
    Receiver rcvr;

    void start() & noexcept {
        ctx->enqueue([this] {
            // now on the context's worker thread:
            ex::set_value(std::move(rcvr));
        });
    }
};

// --- The sender returned by schedule() ---
struct manual_sender {
    using sender_concept = ex::sender_t;
    using completion_signatures = ex::completion_signatures<ex::set_value_t(), ex::set_error_t(std::exception_ptr)>;

    manual_context* ctx;

    template <class Receiver>
    manual_operation<Receiver> connect(Receiver rcvr) const {
        return {ctx, std::move(rcvr)};
    }
};

// --- The scheduler handle ---
class manual_scheduler {
   public:
    using scheduler_concept = ex::scheduler_t;

    explicit manual_scheduler(manual_context* ctx) : ctx_(ctx) {}

    manual_sender schedule() const noexcept { return {ctx_}; }

    bool operator==(const manual_scheduler&) const = default;

   private:
    manual_context* ctx_;
};

manual_scheduler manual_context::get_scheduler() noexcept { return manual_scheduler{this}; }

int main() {
    manual_context ctx;
    ex::scheduler auto sched = ctx.get_scheduler();

    ex::sync_wait(ex::schedule(sched) | ex::then([] { std::puts("hello from the manual context"); }));
}
