// 7x01-promise_v1.cpp
// ===================
// Running example v1: producer side. A worker thread runs fetch_blocking and
// deposits the result into a std::promise<Reply>, the caller reads it.

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

struct Reply {
  int status;
  std::string body;
};

// Synchronous network call. Simulated here with a sleep + a fixed payload.
Reply fetch_blocking(const std::string &replica) {
  std::this_thread::sleep_for(200ms);
  return Reply{200, "payload from " + replica};
}

void fetch_worker(std::promise<Reply> prom, std::string replica) {
  Reply r = fetch_blocking(replica);
  prom.set_value(std::move(r));
}

int main() {
  std::promise<Reply> prom;
  auto fut = prom.get_future();
  std::jthread worker(fetch_worker, std::move(prom), "replica-A");
  // fut now holds a handle to the result the worker will produce.

  Reply r = fut.get();
  std::cout << "status=" << r.status << " body=\"" << r.body << "\"\n";
}
