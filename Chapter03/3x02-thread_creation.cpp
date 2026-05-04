// 3x02-thread_creation.cpp
// Six ways to construct a std::thread: free function, stored lambda,
// embedded lambda, function object, non-static member function,
// and static member function.
// Requires: C++11

#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

// Free function
void task() {
    sync_cout << "Using free function\n";
}

// Function object — operator() overload
struct Worker {
    void operator()() { sync_cout << "Using function object\n"; }
};

// Member functions
struct Service {
    void run()        { sync_cout << "Using non-static member function\n"; }
    static void run_static() { sync_cout << "Using static member function\n"; }
};

int main() {
    // t1: free function pointer
    std::thread t1(task);

    // t2: lambda stored in a variable
    auto lambda = []{ sync_cout << "Using stored lambda\n"; };
    std::thread t2(lambda);

    // t3: embedded lambda (most common in modern C++)
    std::thread t3([]{ sync_cout << "Using embedded lambda\n"; });

    // t4: function object — brace-init avoids the most vexing parse
    std::thread t4{Worker{}};

    // t5: non-static member function
    Service svc;
    std::thread t5(&Service::run, &svc);

    // t6: static member function
    std::thread t6(&Service::run_static);

    t1.join(); t2.join(); t3.join();
    t4.join(); t5.join(); t6.join();
}
