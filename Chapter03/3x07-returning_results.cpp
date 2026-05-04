// 3x07-returning_results.cpp
// ==========================
// Three patterns for getting a result back from a thread:
//   Pattern 1: output via reference argument
//   Pattern 2: output via shared variable protected by a mutex
//   Pattern 3: output via std::promise / std::future (preview — Chapter 7)
//
// Requires: C++11 (pattern 3: C++11)

#include <future>
#include <iostream>
#include <mutex>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

int compute() { return 42; }

int main() {
    // Pattern 1: output via reference argument
    {
        int result = 0;
        std::thread t([&result]{ result = compute(); });
        t.join();
        sync_cout << "Pattern 1 (ref arg): result=" << result << "\n";
    }

    // Pattern 2: output via shared variable protected by a mutex
    {
        int shared_result = 0;
        std::mutex mtx;
        std::thread t([&]{
            std::lock_guard lock(mtx);
            shared_result = compute();
        });
        t.join();
        sync_cout << "Pattern 2 (mutex): result=" << shared_result << "\n";
    }

    // Pattern 3: output via promise/future (preview — Chapter 7)
    {
        std::promise<int> promise;
        auto future = promise.get_future();
        std::thread t([p = std::move(promise)]() mutable {
            p.set_value(compute());
        });
        int val = future.get();  // blocks until set_value() is called
        t.join();
        sync_cout << "Pattern 3 (promise/future): result=" << val << "\n";
    }
}
