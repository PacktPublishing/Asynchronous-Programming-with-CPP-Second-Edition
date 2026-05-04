// 3x17-thread_pool.cpp
// ====================
// Minimal thread pool using std::jthread, stop_token, and condition_variable.
// Amortises thread-creation overhead for workloads with many short tasks.
// Components:
//   - Fixed vector of jthread workers (RAII join on destruction)
//   - Thread-safe task queue (std::queue + mutex)
//   - condition_variable to wake sleeping workers when work arrives
// BEST PRACTICE: size the pool to hardware_concurrency() or a measured optimum.
//
// Requires: C++20

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stop_token>
#include <syncstream>
#include <thread>
#include <vector>

#define sync_cout std::osyncstream(std::cout)

class ThreadPool {
public:
    static constexpr std::size_t DEFAULT_NUM_THREADS = 4;

    explicit ThreadPool(std::size_t n = std::thread::hardware_concurrency()) {
        // hardware_concurrency() may return 0 if the value cannot be determined
        if (n == 0) n = DEFAULT_NUM_THREADS;
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this](std::stop_token st) {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lk(mu_);
                        cv_.wait(lk, [&]{
                            return !tasks_.empty() || st.stop_requested();
                        });
                        // Drain remaining tasks before exiting on a stop request
                        if (tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
    }

    void submit(std::function<void()> f) {
        { std::lock_guard lk(mu_); tasks_.push(std::move(f)); }
        cv_.notify_one();
    }

    ~ThreadPool() {
        // Request stop on each worker, then wake them so the wait predicate
        // re-evaluates and sees st.stop_requested(). Without notify_all() the
        // workers would block forever in cv_.wait() — std::condition_variable
        // does not react to stop_token by itself.
        for (auto& w : workers_) w.request_stop();
        cv_.notify_all();
        // Members destroyed in reverse declaration order: workers_ last,
        // so each jthread joins after the wake-up.
    }

private:
    std::vector<std::jthread>         workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex                        mu_;
    std::condition_variable           cv_;
};

int main() {
    ThreadPool pool(4);

    for (int i = 0; i < 10; ++i)
        pool.submit([i]{ sync_cout << "task " << i << " on thread "
                                   << std::this_thread::get_id() << "\n"; });

    // pool destructor: workers finish their current task, then stop and join
}
