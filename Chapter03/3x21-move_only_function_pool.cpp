// 3x21-move_only_function_pool.cpp
// ================================
// Better task queues using std::move_only_function (C++23). A queue of
// std::function<void()> cannot hold a task that captures a std::unique_ptr,
// because std::function requires a copyable callable. std::move_only_function
// lifts that restriction, so tasks can own their data.
// (Chapter 3, "Better Task Queues using std::move_only_function".)
//
//   g++ -std=c++23 3x21-move_only_function_pool.cpp -o
//   3x21-move_only_function_pool

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <syncstream>
#include <thread>
#include <vector>

#define sync_cout std::osyncstream(std::cout)

struct LargeBuffer {
    std::vector<char> bytes = std::vector<char>(1 << 20, 'x');
};

void process(const LargeBuffer &buffer) {
    sync_cout << "processed " << buffer.bytes.size() << " bytes on thread " << std::this_thread::get_id() << "\n";
}

// The thread pool from 3x17, with std::move_only_function in the queue.
class ThreadPool {
   public:
    explicit ThreadPool(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this](std::stop_token st) {
                while (true) {
                    std::move_only_function<void()> task;
                    {
                        std::unique_lock lk(mu_);
                        cv_.wait(lk, [&] { return !tasks_.empty() || st.stop_requested(); });
                        if (tasks_.empty()) return;  // drain, then exit
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
    }

    void submit(std::move_only_function<void()> f) {
        {
            std::lock_guard lk(mu_);
            tasks_.push(std::move(f));
        }
        cv_.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard lk(mu_);  // no worker can miss the wake-up
            for (auto &w : workers_) w.request_stop();
        }
        cv_.notify_all();
    }

   private:
    // Instead of std::function<void()>:
    std::queue<std::move_only_function<void()>> tasks_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::vector<std::jthread> workers_;
};

int main() {
    ThreadPool pool(2);

    // Submit tasks that capture unique_ptr:
    auto data = std::make_unique<LargeBuffer>();
    pool.submit([d = std::move(data)] { process(*d); });

    // The same task in a std::function would not compile:
    //   std::function<void()> f = [d = std::make_unique<LargeBuffer>()]{
    //   process(*d); };
}
