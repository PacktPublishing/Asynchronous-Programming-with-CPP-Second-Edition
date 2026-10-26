// 7x16-coroutine_preview.cpp
// ==========================
// Preview of the coroutine form of the running fetcher. The mechanics of
// task<T>, awaitables, and schedulers are Chapter 9; this file ships the
// minimum machinery needed to make the chapter's fetch_async() and driver()
// compile and run: a lazy task<T> that can await another task, and an
// awaitable that runs the blocking fetch on a worker thread.
//
// Requires C++20 coroutines (<coroutine>). Compiles with GCC 13 / Clang 16.

#include <chrono>
#include <coroutine>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <semaphore>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

// -- A minimal lazy task<T> -------------------------------------------------
// Awaiting a task starts it; when it finishes it resumes whoever awaited it.
template <typename T = void>
struct task;

namespace detail {
struct promise_base {
    std::exception_ptr exc;
    std::coroutine_handle<> continuation = std::noop_coroutine();

    std::suspend_always initial_suspend() noexcept { return {}; }  // lazy
    void unhandled_exception() noexcept { exc = std::current_exception(); }

    struct final_awaiter {
        bool await_ready() noexcept { return false; }
        template <typename P>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept {
            return h.promise().continuation;  // resume the awaiter
        }
        void await_resume() noexcept {}
    };
    final_awaiter final_suspend() noexcept { return {}; }
};
}  // namespace detail

template <typename T>
struct task {
    struct promise_type : detail::promise_base {
        std::optional<T> value;
        task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void return_value(T v) { value = std::move(v); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
    task(task&& o) noexcept : handle(std::exchange(o.handle, {})) {}
    ~task() {
        if (handle) handle.destroy();
    }

    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
        handle.promise().continuation = caller;
        return handle;  // start the child
    }
    T await_resume() {
        if (handle.promise().exc) std::rethrow_exception(handle.promise().exc);
        return std::move(*handle.promise().value);
    }
};

template <>
struct task<void> {
    struct promise_type : detail::promise_base {
        task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        void return_void() noexcept {}
    };
    std::coroutine_handle<promise_type> handle;
    explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
    task(task&& o) noexcept : handle(std::exchange(o.handle, {})) {}
    ~task() {
        if (handle) handle.destroy();
    }

    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
        handle.promise().continuation = caller;
        return handle;
    }
    void await_resume() {
        if (handle.promise().exc) std::rethrow_exception(handle.promise().exc);
    }
};

// -- The fetcher's pieces -------------------------------------------------------
struct Reply {
    int status;
    std::string body;
};

Reply fetch_blocking(const std::string& replica) {
    std::this_thread::sleep_for(200ms);
    if (replica == "replica-down") throw std::system_error(std::make_error_code(std::errc::host_unreachable), replica);
    return Reply{200, "payload from " + replica};
}

// An awaitable that runs the blocking fetch on a worker thread and resumes the
// coroutine there when it is done. No thread is blocked while it waits.
struct async_fetch {
    std::string replica;
    std::optional<Reply> result{};
    std::exception_ptr exc{};

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        std::jthread([this, h] {
            try {
                result = fetch_blocking(replica);
            } catch (...) {
                exc = std::current_exception();
            }
            h.resume();
        }).detach();
    }
    Reply await_resume() {
        if (exc) std::rethrow_exception(exc);
        return std::move(*result);
    }
};

void process(const Reply& r) { std::cout << "status=" << r.status << " body=\"" << r.body << "\"\n"; }

template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << "\n";
}

// -- The chapter's preview snippet ------------------------------------------
task<Reply> fetch_async(std::string replica) {
    Reply r = co_await async_fetch(replica);  // suspend, no thread blocked
    co_return r;
}

task<void> driver() {
    try {
        Reply r = co_await fetch_async("replica-A");
        process(r);
    } catch (const std::system_error& e) {
        log_error("fetch failed: {}", e.what());
    }
}

// -- Running a task from main -------------------------------------------------
// A small driver coroutine awaits the task and, from its final suspend point,
// releases a semaphore that main waits on. Main then destroys the frame.
struct runner {
    struct promise_type {
        std::binary_semaphore* done = nullptr;
        runner get_return_object() noexcept { return runner{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct signal {
                bool await_ready() noexcept { return false; }
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept { h.promise().done->release(); }
                void await_resume() noexcept {}
            };
            return signal{};
        }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
};

runner run(task<void> t) { co_await t; }

int main() {
    std::binary_semaphore done{0};
    runner r = run(driver());
    r.handle.promise().done = &done;
    r.handle.resume();
    done.acquire();  // blocks until driver() has finished
    r.handle.destroy();
}
