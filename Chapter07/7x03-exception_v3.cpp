// 7x03-exception_v3.cpp
// Running example v3: exception propagation across the thread boundary.
// The worker wraps its body in try/catch and forwards any exception to the
// promise via set_exception; the caller's get() rethrows it.

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

// Simulated network call. This version always fails so we can demonstrate
// exception propagation; toggle the throw to see the success path.
Reply fetch_blocking(const std::string& replica) {
  std::this_thread::sleep_for(200ms);
  throw std::system_error(std::make_error_code(std::errc::host_unreachable),
                          "network error talking to " + replica);
}

void fetch_worker(std::promise<Reply> prom, std::string replica) {
  try {
    Reply r = fetch_blocking(replica);   // may throw
    prom.set_value(std::move(r));
  } catch (...) {
    prom.set_exception(std::current_exception());
  }
}

int main() {
  std::promise<Reply> prom;
  auto fut = prom.get_future();
  std::jthread worker(fetch_worker, std::move(prom), "replica-A");

  try {
    Reply r = fut.get();   // rethrows whatever the worker captured
    std::cout << "status=" << r.status << "\n";
  } catch (const std::system_error& e) {
    std::cerr << "fetch failed: " << e.what() << " (" << e.code() << ")\n";
  }
}
