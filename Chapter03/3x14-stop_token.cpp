// 3x14-stop_token.cpp
// ===================
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
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;

template <typename T>
void show_stop_props(std::string_view name, const T &item) {
    sync_cout << std::boolalpha << name << ": stop_possible=" << item.stop_possible()
              << ", stop_requested=" << item.stop_requested() << '\n';
}

// Stand-ins used by the chapter's shorter listings.
void do_some_work() { sync_cout << "worker: doing some work\n"; }

std::mutex io_mtx;
std::condition_variable_any io_cv;
bool io_ready = false;
void blocking_io_operation() {  // blocks until woken
    std::unique_lock lk(io_mtx);
    io_cv.wait(lk, [] { return io_ready; });
    sync_cout << "blocking_io_operation: interrupted\n";
}
void wake_up_blocking_io() {  // what a stop_callback calls
    {
        std::lock_guard lk(io_mtx);
        io_ready = true;
    }
    io_cv.notify_all();
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
    // --- stop_token in practice ---
    {
        std::jthread worker([](std::stop_token st) {
            while (!st.stop_requested()) {
                do_some_work();
                std::this_thread::sleep_for(100ms);
            }
            // Clean up resources here before returning
        });

        std::this_thread::sleep_for(2s);
        worker.request_stop();  // signal the thread to stop
                                // jthread destructor joins automatically
    }

    // --- stop_callback: wake a blocking call ---
    {
        std::jthread worker([](std::stop_token st) {
            std::stop_callback cb(st, [] { wake_up_blocking_io(); });
            blocking_io_operation();  // will be interrupted via callback
        });
        std::this_thread::sleep_for(200ms);
        worker.request_stop();  // runs the callback, which wakes the I/O wait
    }

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
    auto stopper = std::thread(
        [](std::stop_source source) {
            std::this_thread::sleep_for(500ms);
            sync_cout << "watchdog: requesting stop for worker2\n";
            source.request_stop();
        },
        src);

    stopper.join();
    worker2.join();
    show_stop_props("stop_source after", src);

    // --- stop_callback: react to cancellation (e.g. wake a blocking call) ---
    auto worker3 = std::jthread(func_with_stop_token);

    // Registered while the thread is running — fires when request_stop() is
    // called
    std::stop_callback cb(worker3.get_stop_token(), [] {
        sync_cout << "stop_callback: invoked on thread " << std::this_thread::get_id() << '\n';
    });

    // Scoped callback: destroyed before stop — will NOT fire
    {
        std::stop_callback scoped_cb(worker3.get_stop_token(), [] { sync_cout << "This will not execute\n"; });
    }  // scoped_cb destroyed here — callback deregistered

    std::this_thread::sleep_for(500ms);
    worker3.request_stop();
    worker3.join();

    // Post-stop callback: fires immediately at construction if stop already
    // requested After worker.request_stop() has been called:
    std::stop_callback late_cb(worker3.get_stop_token(),
                               [] { sync_cout << "Fires immediately, stop already requested\n"; });
}
