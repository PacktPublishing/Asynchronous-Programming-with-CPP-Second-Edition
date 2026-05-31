// 7x19-bridging_coroutine.cpp
// Bridge a legacy std::future-returning API into a coroutine by polling
// wait_for and yielding between checks. No OS thread is permanently blocked,
// but poll granularity becomes cancellation latency.
//
// Uses the minimal task<T> machinery from 7x16-coroutine_preview.cpp.

#include <chrono>
#include <coroutine>
#include <exception>
#include <future>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

// -- Minimal task<T> (same as 7x16) -----------------------------------------

template <typename T>
struct task {
  struct promise_type {
    std::optional<T>  value;
    std::exception_ptr exc;
    task get_return_object() {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(T v) { value = std::move(v); }
    void unhandled_exception() { exc = std::current_exception(); }
  };
  std::coroutine_handle<promise_type> handle;
  explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
  task(task&& o) noexcept : handle(std::exchange(o.handle, {})) {}
  ~task() { if (handle) handle.destroy(); }
  T get() {
    if (handle.promise().exc) std::rethrow_exception(handle.promise().exc);
    return std::move(*handle.promise().value);
  }
};

// -- "sleep_for" awaitable: yields to a worker that sleeps and resumes ------

struct sleep_for {
  std::chrono::milliseconds dur;
  bool await_ready() const noexcept { return dur.count() == 0; }
  void await_suspend(std::coroutine_handle<> h) const {
    std::jthread([h, d = dur]{
      std::this_thread::sleep_for(d);
      h.resume();
    }).detach();
  }
  void await_resume() const noexcept {}
};

// -- Bridge: poll the legacy future from inside a coroutine -----------------

template <typename T>
task<T> awaitable_future(std::future<T> f) {
  while (f.wait_for(0ms) != std::future_status::ready) {
    co_await sleep_for{1ms};
  }
  co_return f.get();
}

// -- Demo: a "legacy" API that returns std::future<int> ---------------------

std::future<int> legacy_compute() {
  return std::async(std::launch::async, []{
    std::this_thread::sleep_for(200ms);
    return 7;
  });
}

int main() {
  auto t = awaitable_future(legacy_compute());
  while (!t.handle.done()) std::this_thread::sleep_for(5ms);
  std::cout << "bridged via coroutine: " << t.get() << "\n";
}
