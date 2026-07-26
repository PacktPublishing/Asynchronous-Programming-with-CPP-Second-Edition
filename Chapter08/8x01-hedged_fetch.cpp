// 8x01-hedged_fetch.cpp
// =====================
// Flagship example for Chapter 8. Fan out the same request to N replicas with
// std::async, return the first successful reply, ignore the rest.
// Demonstrates: launch policy, futures vector, automatic exception capture,
// no when_any (busy-poll), no cancellation (losers keep running).

#include <chrono>
#include <future>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

struct Reply {
  int status;
  std::string body;
};

// Simulated network call. Each replica answers after a random delay between
// 50 and 800 ms; "replica-fail" throws to exercise the catch path.
Reply fetch_blocking(std::string replica) {
  thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist{50, 800};
  std::this_thread::sleep_for(std::chrono::milliseconds{dist(rng)});
  if (replica == "replica-fail")
    throw std::runtime_error{"replica-fail: connection refused"};
  return Reply{200, "payload from " + replica};
}

std::optional<Reply> hedged_fetch(std::vector<std::string> replicas,
                                  std::chrono::milliseconds budget) {

  // [1] One std::async per replica, all launched in parallel.
  std::vector<std::future<Reply>> futs;
  for (auto &r : replicas)
    futs.push_back(std::async(std::launch::async, // [2]
                              fetch_blocking, r));

  // [3] Poll for the first ready future, or give up at the deadline.
  const auto deadline = std::chrono::steady_clock::now() + budget;
  while (std::chrono::steady_clock::now() < deadline) {
    for (auto &f : futs) {
      if (!f.valid())
        continue;
      if (f.wait_for(0s) == std::future_status::ready) {
        try {
          return f.get();
        } // [4] exception capture
        catch (...) {
          continue;
        } // try the next replica
      }
    }
    std::this_thread::sleep_for(1ms); // [5] no when_any
  }
  return std::nullopt; // [6] losers keep running
}

int main() {
  std::vector<std::string> replicas{"replica-A", "replica-B", "replica-fail",
                                    "replica-D"};

  const auto t0 = std::chrono::steady_clock::now();
  auto reply = hedged_fetch(replicas, 500ms);
  const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0);

  if (reply)
    std::cout << "winner status=" << reply->status << " body=\"" << reply->body
              << "\""
              << " in " << dt.count() << " ms\n";
  else
    std::cout << "no replica answered within budget"
              << " (" << dt.count() << " ms)\n";

  // Note: at this point hedged_fetch has returned, but the std::async futures
  // it created are out of scope, and their destructors join their threads.
  // The losing replicas are *still running* until then.
}
