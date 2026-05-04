// 3x12-sleep_yield.cpp
// Controlling thread scheduling:
//   sleep_for  — block for at least a given duration
//   sleep_until — block until a specific time point (use steady_clock)
//   yield      — hint to the OS to run another thread (implementation-defined)
// NOTE: yield() is not a precise timing mechanism; use sleep_for for that.
// Requires: C++11

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;
using namespace std::chrono;

int main() {
    // sleep_for: block for at least 200 ms
    std::jthread t1([] {
        sync_cout << "t1: sleeping for 200ms\n";
        std::this_thread::sleep_for(200ms);
        sync_cout << "t1: awake\n";
    });

    // sleep_until: block until a specific time point
    std::jthread t2([] {
        auto wake_at = steady_clock::now() + 300ms;
        sync_cout << "t2: sleeping until time point\n";
        std::this_thread::sleep_until(wake_at);
        sync_cout << "t2: awake\n";
    });

    // yield: voluntarily give up time slice; may have no effect if no other
    // thread of the same priority is ready
    std::jthread t3([] {
        for (int i = 0; i < 4; ++i) {
            if (std::rand() % 2) {
                sync_cout << "t3: working\n";
            } else {
                sync_cout << "t3: yielding\n";
                std::this_thread::yield();
            }
        }
    });

    // jthreads join automatically on destruction
}
