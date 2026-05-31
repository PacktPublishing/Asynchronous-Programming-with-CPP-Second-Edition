// 7x12-no_stop_token.cpp
// Demonstrates the "no std::stop_token" limitation: the only way to abandon
// a wait on a stop request is to poll wait_for in a loop. Poll granularity
// becomes cancellation latency, and the producer keeps running anyway.

#include <chrono>
#include <future>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

using namespace std::chrono_literals;

struct Reply {
  int status;
  std::string body;
};

Reply fetch_blocking(const std::string& replica) {
  // Pretend this takes a long time.
  std::this_thread::sleep_for(2s);
  return Reply{200, "payload from " + replica};
}

// Poll wait_for in a loop, checking the stop token between polls.
// On stop, the producer is still running — no way to cancel it from here.
void wait_with_stop(std::future<Reply>& fut, std::stop_token st) {
  while (!st.stop_requested()) {
    if (fut.wait_for(50ms) == std::future_status::ready) return;
  }
  std::cerr << "wait_with_stop: stop requested; producer still running\n";
}

int main() {
  std::promise<Reply> prom;
  auto fut = prom.get_future();
  std::jthread worker([p = std::move(prom)]() mutable {
    p.set_value(fetch_blocking("replica-A"));
  });

  std::stop_source stop;
  std::jthread canceller([&]{
    std::this_thread::sleep_for(200ms);
    stop.request_stop();
  });

  wait_with_stop(fut, stop.get_token());

  if (fut.valid() && fut.wait_for(0s) == std::future_status::ready) {
    Reply r = fut.get();
    std::cout << "got reply anyway: " << r.body << "\n";
  } else {
    std::cout << "abandoned the wait\n";
  }
}
