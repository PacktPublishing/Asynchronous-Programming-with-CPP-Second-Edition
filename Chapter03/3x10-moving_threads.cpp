// 3x10-moving_threads.cpp
// std::thread is move-only: two objects cannot represent the same OS thread.
// After a move the source object no longer represents any thread.
// Requires: C++11

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

void task() { std::this_thread::sleep_for(200ms); }

int main() {
    std::thread t1(task);
    std::cout << "t1 ID before move: " << t1.get_id() << "\n";

    std::thread t2 = std::move(t1);   // ownership transferred to t2
    std::cout << "t1 ID after move:  " << t1.get_id() << "\n";  // default (empty)
    std::cout << "t2 ID after move:  " << t2.get_id() << "\n";  // same as t1 had

    t2.join();
}
