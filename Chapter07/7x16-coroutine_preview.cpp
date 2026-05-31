// 7x16-coroutine_preview.cpp
// Preview of the coroutine form of the running fetcher. The mechanics of
// task<T>, awaitables, and schedulers are Chapter 9; this file ships the
// minimum machinery needed to make the fetch_async() body compile and run.
//
// Requires C++20 coroutines (<coroutine>). Compiles with GCC 13 / Clang 16.

#include <chrono>
#include <coroutine>
#include <exception>
#include <future>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

// -- Minimal eager task<T> ---------------------------------------------------
// Real coroutine task types are lazy and composable; this one is the smallest
// thing that demonstrates co_await + co_return for the chapter preview.

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

// -- A simple awaitable that runs work on a worker thread --------------------

template <typename F>
struct run_async {
  F        work;
  using R  = decltype(work());
  std::optional<R>   result{};
  std::exception_ptr exc{};

  bool await_ready() noexcept { return false; }

  void await_suspend(std::coroutine_handle<> h) {
    std::jthread([this, h]{
      try { result = work(); }
      catch (...) { exc = std::current_exception(); }
      h.resume();
    }).detach();
  }

  R await_resume() {
    if (exc) std::rethrow_exception(exc);
    return std::move(*result);
  }
};

// -- The chapter's preview snippet, made runnable ----------------------------

struct Reply {
  int status;
  std::string body;
};

Reply fetch_blocking(const std::string& replica) {
  std::this_thread::sleep_for(200ms);
  return Reply{200, "payload from " + replica};
}

task<Reply> fetch_async(std::string replica) {
  Reply r = co_await run_async{[replica]{ return fetch_blocking(replica); }};
  co_return r;
}

int main() {
  auto t = fetch_async("replica-A");

  // Wait until the eager task has finished. With a real scheduler-aware task
  // you would await it; for this minimal type we just spin until it is done.
  while (!t.handle.done()) std::this_thread::sleep_for(10ms);

  try {
    Reply r = t.get();
    std::cout << "status=" << r.status << " body=\"" << r.body << "\"\n";
  } catch (const std::system_error& e) {
    std::cerr << "fetch failed: " << e.what() << "\n";
  }
}
