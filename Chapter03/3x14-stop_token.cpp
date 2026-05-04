// 3x14-stop_token.cpp
// C++20 cooperative cancellation: stop_token, stop_source, stop_callback.
//
//   stop_source  — write side: issues a stop request
//   stop_token   — read side: checked by the thread
//   stop_callback — registers a callback fired when stop is requested
//
// BEST PRACTICE: check stop_requested() at the top of the loop body,
// not just at the bottom, to react without completing a full extra iteration.
// Requires: C++20

#include <chrono>
#include <iostream>
#include <stop_token>
#include <string_view>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;

template <typename T>
void show_stop_props(std::string_view name, const T& item) {
    sync_cout << std::boolalpha << name
              << ": stop_possible=" << item.stop_possible()
              << ", stop_requested=" << item.stop_requested() << '\n';
}

void func_with_stop_token(std::stop_token st) {
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(300ms);
        if (st.stop_requested()) {
            sync_cout << "worker: stopping as requested\n";
            return;
        }
        sync_cout << "worker: going back to sleep (iteration " << i << ")\n";
    }
}

int main() {
    // --- stop_token: checking stop_requested() ---
    auto worker1 = std::jthread(func_with_stop_token);
    std::stop_token tok = worker1.get_stop_token();
    show_stop_props("before request", tok);

    std::this_thread::sleep_for(1s);
    worker1.request_stop();
    worker1.join();
    show_stop_props("after request", tok);

    // --- stop_source: third-party cancellation ---
    auto worker2 = std::jthread(func_with_stop_token);
    std::stop_source src = worker2.get_stop_source();
    show_stop_props("stop_source before", src);

    // A separate "watchdog" thread requests a stop on behalf of a third party
    auto stopper = std::thread([](std::stop_source source) {
        std::this_thread::sleep_for(500ms);
        sync_cout << "watchdog: requesting stop for worker2\n";
        source.request_stop();
    }, src);

    stopper.join();
    worker2.join();
    show_stop_props("stop_source after", src);

    // --- stop_callback: react to cancellation (e.g. wake a blocking call) ---
    auto worker3 = std::jthread(func_with_stop_token);

    // Registered while the thread is running — fires when request_stop() is called
    std::stop_callback cb(worker3.get_stop_token(), []{
        sync_cout << "stop_callback: invoked on thread "
                  << std::this_thread::get_id() << '\n';
    });

    // Scoped callback: destroyed before stop — will NOT fire
    {
        std::stop_callback scoped_cb(worker3.get_stop_token(), []{
            sync_cout << "scoped_cb: this will NOT execute\n";
        });
    }  // scoped_cb deregistered here

    std::this_thread::sleep_for(500ms);
    worker3.request_stop();
    worker3.join();

    // Post-stop callback: fires immediately at construction if stop already requested
    std::stop_callback late_cb(worker3.get_stop_token(), []{
        sync_cout << "late_cb: fires immediately — stop already requested\n";
    });
}
