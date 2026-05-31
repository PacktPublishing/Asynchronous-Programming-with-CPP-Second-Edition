// 7x13-jthread_promise_pitfall.cpp
// Demonstrates the silent-deadlock pitfall: a jthread that exits early without
// satisfying the promise leaves the consumer to discover broken_promise only
// after the jthread has actually destroyed and dropped the promise. If the
// worker is doing slow setup before the early return, get() blocks until the
// setup completes — even though the failure was decided up front.
//
// Run: an exit-on-precondition worker that "does setup" before bailing.
//      Observe that get() blocks for the full setup duration.

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;

struct Reply { int status; };

int main() {
  bool some_precondition_failed = true;

  std::promise<Reply> prom;
  auto fut = prom.get_future();

  auto t0 = std::chrono::steady_clock::now();

  std::jthread worker([prom = std::move(prom),
                       some_precondition_failed]() mutable {
    // Imagine the worker does some setup, *then* discovers it cannot proceed.
    std::this_thread::sleep_for(500ms);
    if (some_precondition_failed) {
      std::cerr << "[worker] precondition failed; returning early\n";
      return;   // promise destroyed without set_value/set_exception
    }
    prom.set_value(Reply{200});
  });

  try {
    Reply r = fut.get();   // blocks for ~500 ms even though failure is up front
    std::cout << "status=" << r.status << "\n";
  } catch (const std::future_error& e) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    std::cerr << "broken_promise after " << elapsed << " ms: " << e.what() << "\n";
    std::cerr << "(failure was decided immediately; we waited for setup anyway)\n";
  }
}
