// 3x13-atomic_flag_cancellation.cpp
// =================================
// Pre-C++20 cancellation pattern: a shared std::atomic_bool flag checked
// by the thread each iteration. This is the boilerplate that stop_token
// (see 3x14) was designed to replace.
//
// Requires: C++20 (jthread; the pattern itself works from C++11)

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;

class Counter {
    using Callback = std::function<void()>;
public:
    explicit Counter(Callback callback) {
        // Move the callback into the lambda — capturing it by reference would
        // dangle as soon as this constructor returns.
        t_ = std::jthread([this, cb = std::move(callback)] {
            while (running_.load()) {
                cb();
                std::this_thread::sleep_for(1s);
            }
        });
    }
    void stop() { running_.store(false); }

private:
    std::jthread    t_;
    std::atomic_bool running_{true};
};

int main() {
    Counter counter([]{ sync_cout << "Counter: tick\n"; });

    std::this_thread::sleep_for(3s);

    counter.stop();
    // counter destructor joins t_ automatically
}
