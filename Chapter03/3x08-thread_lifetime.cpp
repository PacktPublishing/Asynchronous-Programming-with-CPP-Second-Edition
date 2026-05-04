// 3x08-thread_lifetime.cpp
// join(), joinable(), swap(), and move semantics for std::thread.
// IMPORTANT: destroying a joinable std::thread calls std::terminate.
//            std::jthread (3x11) eliminates this hazard.
// Requires: C++11

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main() {
    // Default-constructed thread: not joinable
    std::thread t1;
    std::cout << "t1 joinable (before assign): " << t1.joinable() << "\n";  // false

    std::thread t2([]{ std::this_thread::sleep_for(100ms); });

    // swap: t1 takes ownership of t2's thread; t2 becomes empty
    t1.swap(t2);
    std::cout << "t1 joinable (after swap):    " << t1.joinable() << "\n";  // true
    std::cout << "t2 joinable (after swap):    " << t2.joinable() << "\n";  // false

    t1.join();
    std::cout << "t1 joinable (after join):    " << t1.joinable() << "\n";  // false

    // Move: transfer ownership from t3 to t4
    std::thread t3([]{ std::this_thread::sleep_for(100ms); });
    std::thread t4 = std::move(t3);
    // t3 is no longer joinable
    std::cout << "t3 joinable (after move):    " << t3.joinable() << "\n";  // false
    std::cout << "t4 joinable (after move):    " << t4.joinable() << "\n";  // true
    t4.join();
}
