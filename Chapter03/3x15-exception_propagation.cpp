// 3x15-exception_propagation.cpp
// Propagating an exception from a worker thread to the calling thread.
// Exceptions do not cross thread boundaries on their own; use
// std::current_exception() to capture and std::rethrow_exception() to re-throw.
// BEST PRACTICE: prefer std::promise/std::future (Chapter 7) which handle
// this boilerplate automatically.
// Requires: C++11

#include <exception>
#include <iostream>
#include <mutex>
#include <thread>

std::exception_ptr g_exception;
std::mutex g_exc_mutex;

void risky_operation() {
    throw std::runtime_error("error inside worker thread");
}

int main() {
    std::thread worker([&]{
        try {
            risky_operation();
        } catch (...) {
            std::lock_guard lk(g_exc_mutex);
            g_exception = std::current_exception();
        }
    });

    worker.join();

    if (g_exception) {
        try {
            std::rethrow_exception(g_exception);
        } catch (const std::exception& e) {
            std::cerr << "Worker failed: " << e.what() << "\n";
        }
    }
}
