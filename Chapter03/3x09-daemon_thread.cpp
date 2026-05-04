// 3x09-daemon_thread.cpp
// detach() lets a thread run as a background daemon.
// Once detached, the thread cannot be joined; resources are reclaimed
// by the OS when the thread finishes or the process exits.
// WARNING: if a detached thread accesses objects destroyed before it finishes,
//          the result is undefined behaviour. Prefer jthread + stop_token.
// Requires: C++20

#include <chrono>
#include <iostream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)
using namespace std::chrono_literals;

void daemonThread() {
    sync_cout << "Daemon: starting\n";
    for (int i = 3; i > 0; --i) {
        sync_cout << "Daemon: running (" << i << "s left)\n";
        std::this_thread::sleep_for(1s);
    }
    sync_cout << "Daemon: exiting\n";
}

int main() {
    std::thread t(daemonThread);
    t.detach();

    // main thread outlives the daemon so it can finish cleanly
    std::this_thread::sleep_for(4s);
    sync_cout << "Main: exiting\n";
}
