// 12x09-custom_scheduler.cpp
// ==========================
// Building a custom execution context and scheduler from scratch:
// A single background thread draining a queue, a scheduler handle, and the
// sender / operation-state pair its schedule() returns.
// Demonstrates the three pieces every scheduler owes the framework.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include 12x09-custom_scheduler.cpp
//       -o 12x09-custom_scheduler

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

#include <stdexec/execution.hpp>

namespace ex = stdexec;

// Context: Owns a worker thread and a task queue
class manual_context {
public:
  manual_context() : worker_([this] { run(); }) {}

  ~manual_context() {
    {
      std::lock_guard lk{mtx_};
      stop_ = true;
    }
    cv_.notify_one();
    worker_.join(); // Drain queue and then join
  }

  void enqueue(std::function<void()> task) {
    {
      std::lock_guard lk{mtx_};
      queue_.push_back(std::move(task));
    }
    cv_.notify_one();
  }

  class scheduler;
  scheduler get_scheduler() noexcept;

private:
  void run() {
    for (;;) {
      std::function<void()> task;
      {
        std::unique_lock lk{mtx_};
        cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
        if (stop_ && queue_.empty()) {
          return;
        }
        task = std::move(queue_.front());
        queue_.pop_front();
      }
      task(); // Notified and task exists.
    }
  }

  std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> queue_;
  bool stop_{false};
  std::jthread worker_;
};

// Operation state: start() enqueues work that completes the receiver
template <class Receiver> struct manual_operation {
  manual_context *ctx;
  Receiver rcvr;

  void start() & noexcept {
    ctx->enqueue([this] {
      // Here on the context's worker thread
      ex::set_value(std::move(rcvr));
    });
  }
};

// Sender returned by schedule()
struct manual_sender {
  using sender_concept = ex::sender_t;
  using completion_signatures =
      ex::completion_signatures<ex::set_value_t(),
                                ex::set_error_t(std::exception_ptr)>;

  manual_context *ctx;

  template <class Receiver>
  manual_operation<Receiver> connect(Receiver rcvr) const {
    return {ctx, std::move(rcvr)};
  }
};

// Scheduler handle
class manual_context::scheduler {
public:
  using scheduler_concept = ex::scheduler_t;
  explicit scheduler(manual_context *ctx) : ctx_(ctx) {}
  manual_sender schedule() const noexcept { return {ctx_}; }
  bool operator==(const scheduler &) const = default;

private:
  manual_context *ctx_;
};

manual_context::scheduler manual_context::get_scheduler() noexcept {
  return scheduler{this};
}

int main() {
  manual_context ctx;
  ex::scheduler auto sched = ctx.get_scheduler();

  ex::sync_wait(ex::schedule(sched)
              | ex::then([] {
                std::puts("hello from the manual context");
            }));
}
