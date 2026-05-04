// 3x18-cpp23_features.cpp
// C++23 additions relevant to thread management:
//
//   1. std::condition_variable_any::wait() overload accepting std::stop_token
//      — a thread can be woken by either a notification OR a stop request.
//
//   2. std::move_only_function — replaces std::function in task queues when
//      tasks capture move-only types (e.g. std::unique_ptr).
//
// Requires: C++23

#include <condition_variable>
#include <functional>   // std::move_only_function (C++23)
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

// --- Feature 1: condition_variable_any + stop_token ---

void demo_cva_stop_token() {
    std::mutex mu;
    bool data_ready = false;

    std::jthread worker([&](std::stop_token st) {
        std::unique_lock lk(mu);
        // Wait until data_ready OR a stop is requested — whichever comes first
        bool notified = std::condition_variable_any{}.wait(
            lk, st, [&]{ return data_ready; }
        );
        if (!notified)
            sync_cout << "cva demo: stopped before data arrived\n";
        else
            sync_cout << "cva demo: data is ready\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Request stop instead of setting data_ready — worker exits via stop_token
    worker.request_stop();
}

// --- Feature 2: std::move_only_function in a task queue ---

class TaskQueue {
public:
    // move_only_function allows tasks that capture unique_ptr or other move-only types
    void push(std::move_only_function<void()> f) {
        tasks_.push(std::move(f));
    }

    void run_all() {
        while (!tasks_.empty()) {
            tasks_.front()();
            tasks_.pop();
        }
    }

private:
    std::queue<std::move_only_function<void()>> tasks_;
};

void demo_move_only_function() {
    TaskQueue q;

    // Task captures a unique_ptr — impossible with std::function
    auto data = std::make_unique<std::string>("move-only payload");
    q.push([d = std::move(data)]{
        sync_cout << "move_only_function task: " << *d << "\n";
    });

    q.run_all();
}

int main() {
    demo_cva_stop_token();
    demo_move_only_function();
}
