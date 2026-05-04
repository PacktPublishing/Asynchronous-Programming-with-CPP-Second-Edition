// 3x06-passing_arguments.cpp
// ==========================
// Four argument-passing patterns for std::thread:
//   - by value (safe, costs a copy)
//   - by reference via std::ref / std::cref
//   - via lambda capture
//   - move-only types via std::move
//
// Requires: C++11

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

#define sync_cout std::osyncstream(std::cout)

// Passing by value — function receives its own copies; no data races possible
void process(std::string data, int count) {
    sync_cout << "by value: data=\"" << data
              << "\", count=" << count << "\n";
}

// Passing by mutable reference — caller must ensure the object outlives the thread
void append(std::string& s) { s += " world"; }

// Passing by const reference — read-only access
void print_vec(const std::vector<int>& v) {
    sync_cout << "by cref: vec size=" << v.size() << "\n";
}

// Move-only type — takes ownership inside the thread
void consume(std::unique_ptr<std::string> p) {
    sync_cout << "by move: *p=\"" << *p << "\"\n";
}

int main() {
    // By value: input is copied into the thread
    std::string input = "hello";
    std::thread t1(process, input, 42);
    t1.join();

    // By mutable reference: std::ref wraps into reference_wrapper
    std::string s = "hello";
    std::thread t2(append, std::ref(s));
    t2.join();
    sync_cout << "after ref thread: s=\"" << s << "\"\n";

    // By const reference: std::cref
    std::vector<int> v{1, 2, 3};
    std::thread t3(print_vec, std::cref(v));
    t3.join();

    // Via lambda capture — most idiomatic in modern C++
    int result = 0;
    std::thread t4([&result]{ result = 42; });
    t4.join();
    sync_cout << "lambda capture result=" << result << "\n";

    // Move-only type: std::move transfers ownership; ptr is empty after this
    auto ptr = std::make_unique<std::string>("moved string");
    std::thread t5(consume, std::move(ptr));
    t5.join();
    // ptr is now empty
    sync_cout << "ptr after move: " << (ptr ? *ptr : "(empty)") << "\n";
}
