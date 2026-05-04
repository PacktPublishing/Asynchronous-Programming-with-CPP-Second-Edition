// 3x11-jthread_raii.cpp
// std::jthread (C++20): RAII joining thread.
// - Joins automatically in its destructor (even if an exception is thrown).
// - Multiple jthreads are destroyed in reverse construction order (LIFO).
// Requires: C++20

#include <chrono>
#include <functional>
#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;

// Wrapper that logs construction and destruction to show LIFO ordering
class JthreadWrapper {
public:
    JthreadWrapper(const std::function<void(const std::string&)>& func,
                   const std::string& name)
        : t_(func, name), name_(name) {
        sync_cout << "Thread " << name_ << " being created\n";
    }
    ~JthreadWrapper() {
        sync_cout << "Thread " << name_ << " being destroyed\n";
    }
private:
    std::jthread t_;
    std::string  name_;
};

void worker(const std::string& name) {
    sync_cout << "Thread " << name << " starting\n";
    std::this_thread::sleep_for(1s);
    sync_cout << "Thread " << name << " finishing\n";
}

int main() {
    // Basic jthread: no explicit join() needed
    {
        std::jthread t([]{ std::this_thread::sleep_for(100ms); });
        sync_cout << "jthread ID: " << t.get_id() << "\n";
    }  // destructor joins here automatically

    // LIFO destruction order
    JthreadWrapper t1(worker, "t1");
    JthreadWrapper t2(worker, "t2");
    JthreadWrapper t3(worker, "t3");
    std::this_thread::sleep_for(2s);
    sync_cout << "Main thread exiting\n";
    // t3 destroyed first, then t2, then t1
}
