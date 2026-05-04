// 3x01-concurrent_file_processor.cpp
// A complete motivating example: concurrent word-frequency counter.
// Demonstrates: jthread, stop_token, thread_local, osyncstream, mutex,
// hardware_concurrency, and a minimal thread pool — all in one program.
// Requires: C++20, a ./data directory with text files to process.

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <stop_token>    // C++20
#include <syncstream>    // C++20
#include <thread>
#include <vector>

using namespace std::chrono_literals;
#define sync_cout std::osyncstream(std::cout)  // [1] Synchronized output

// [2] Thread-local counter — each thread owns its copy
thread_local std::size_t words_processed{0};

// [3] Shared results, protected by a mutex
std::map<std::string, std::size_t> global_word_counts;
std::mutex results_mutex;

// [4] Core task: count words in one file
void count_words(const std::filesystem::path& file,
                 std::stop_token st) {
    if (st.stop_requested()) return;   // honor cancellation
    std::ifstream f(file);
    std::string word;
    std::map<std::string, std::size_t> local;
    while (!st.stop_requested() && f >> word) {
        ++local[word];
        ++words_processed;   // thread-local, no lock needed
    }
    // Merge into global map
    std::lock_guard lock(results_mutex);
    for (auto& [w, n] : local) global_word_counts[w] += n;
    sync_cout << "[thread " << std::this_thread::get_id()
              << "] processed " << words_processed << " words\n";
}

// [5] Minimal thread pool — limited to hardware concurrency
class ThreadPool {
public:
    explicit ThreadPool(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this](std::stop_token st) {
                while (!st.stop_requested()) {
                    std::function<void(std::stop_token)> task;
                    {
                        std::unique_lock lk(mu_);
                        cv_.wait(lk, [&]{ return !tasks_.empty() || st.stop_requested(); });
                        if (st.stop_requested()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task(st);
                }
            });
    }

    void submit(std::function<void(std::stop_token)> t) {
        { std::lock_guard lk(mu_); tasks_.push(std::move(t)); }
        cv_.notify_one();
    }

    void stop() {
        // Request stop and wake all workers so cv_.wait() re-evaluates and
        // sees st.stop_requested() — std::condition_variable does not react
        // to stop_token by itself.
        for (auto& w : workers_) w.request_stop();
        cv_.notify_all();
    }

    ~ThreadPool() { stop(); }   // safety net if caller forgot to call stop()

private:
    std::vector<std::jthread> workers_;  // [6] jthread: RAII join
    std::queue<std::function<void(std::stop_token)>> tasks_;
    std::mutex mu_;
    std::condition_variable cv_;
};

int main() {
    const auto n = std::thread::hardware_concurrency();  // [7]
    sync_cout << "Hardware concurrency: " << n << "\n";

    ThreadPool pool(n ? n : 4);

    // Submit one task per file
    for (auto& entry : std::filesystem::directory_iterator("./data"))
        pool.submit([p = entry.path()](std::stop_token st) {
            count_words(p, st);
        });

    // Wait 5 seconds, then ask all threads to stop gracefully
    std::this_thread::sleep_for(5s);
    pool.stop();   // [8] cooperative cancellation

    // pool destructor joins all jthreads automatically
    sync_cout << "Unique words found: " << global_word_counts.size() << "\n";
}
