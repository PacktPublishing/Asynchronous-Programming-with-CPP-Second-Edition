// 3x16-thread_local_storage.cpp
// =============================
// Thread-local storage (TLS): each thread owns an independent copy of val.
// No synchronisation overhead — reads and writes are always thread-safe
// because no thread can see another thread's copy.
// NOTE: TLS increases memory usage proportionally to the number of threads.
//
// Requires: C++11

#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

thread_local int val = 0;

void setValue(int newval) { val = newval; }
void printValue(const std::string& name) {
    sync_cout << name << ": val=" << val << "\n";
}

void multiplyByTwo(int arg, const std::string& name) {
    setValue(arg);
    val *= 2;
    printValue(name);
}

int main() {
    val = 1;  // main thread's own copy

    std::thread t1(multiplyByTwo, 1, "t1");  // t1: 1*2 = 2
    std::thread t2(multiplyByTwo, 2, "t2");  // t2: 2*2 = 4
    std::thread t3(multiplyByTwo, 3, "t3");  // t3: 3*2 = 6

    t1.join(); t2.join(); t3.join();

    // main thread's val was set to 1 before launching threads and is unchanged
    std::cout << "main: val=" << val << "\n";  // 1
}
