// 7x04-deadline_v4.cpp
// Running example v4: enforce a deadline on the fetch. wait_for returns a
// future_status; we inspect it before calling get() so we never block past
// the budget. The worker keeps running on timeout — see the chapter's
// "no std::stop_token integration" subsection for why this is unavoidable.

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>

using namespace std::chrono_literals;

struct Reply {
  int status;
  std::string body;
};

// Simulated network call: takes 800 ms, longer than our 500 ms budget,
// so this run will time out.
Reply fetch_blocking(const std::string& replica) {
  std::this_thread::sleep_for(800ms);
  return Reply{200, "payload from " + replica};
}

void fetch_worker(std::promise<Reply> prom, std::string replica) {
  try {
    Reply r = fetch_blocking(replica);
    prom.set_value(std::move(r));
  } catch (...) {
    prom.set_exception(std::current_exception());
  }
}

int main() {
  std::promise<Reply> prom;
  auto fut = prom.get_future();
  std::jthread worker(fetch_worker, std::move(prom), "replica-A");

  if (fut.wait_for(500ms) == std::future_status::ready) {
    try {
      Reply r = fut.get();
      std::cout << "status=" << r.status << " body=\"" << r.body << "\"\n";
    } catch (const std::system_error& e) {
      std::cerr << "fetch failed: " << e.what() << "\n";
    }
  } else {
    std::cerr << "WARN: replica-A exceeded 500ms budget\n";
    // The jthread keeps running; its eventual result is dropped when fut goes
    // out of scope.
  }
}
