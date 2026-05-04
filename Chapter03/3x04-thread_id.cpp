// 3x04-thread_id.cpp
// Retrieving and printing thread identifiers.
// std::thread::id is printable, comparable, hashable, and usable as a map key.
// NOTE: IDs can be reused after a thread has finished and been joined.
// Requires: C++11

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main() {
    std::cout << "Main thread ID: " << std::this_thread::get_id() << "\n";

    std::thread t([]{ std::this_thread::sleep_for(1s); });

    // Retrieve the ID from the thread object before it is joined
    std::cout << "Child thread ID: " << t.get_id() << "\n";

    t.join();

    // After join the thread object no longer represents a running thread
    std::cout << "After join, child ID: " << t.get_id() << "\n";
}
