// 7x09-expected_pairing.cpp
// =========================
// Modern pairing: std::expected<Reply, FetchError> as the value channel.
// Domain-level failures travel as values; only true machinery failures throw.
// Requires C++23 (-std=c++23).

#include <chrono>
#include <expected>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

struct Reply {
  int status;
  std::string body;
};

enum class FetchError { NotFound, Timeout, Corrupted };

const char *to_string(FetchError e) {
  switch (e) {
  case FetchError::NotFound:
    return "NotFound";
  case FetchError::Timeout:
    return "Timeout";
  case FetchError::Corrupted:
    return "Corrupted";
  }
  return "?";
}

// Returns std::expected — never throws for expected failures.
std::expected<Reply, FetchError> try_fetch(const std::string &replica) {
  std::this_thread::sleep_for(150ms);
  if (replica == "replica-missing")
    return std::unexpected(FetchError::NotFound);
  return Reply{200, "payload from " + replica};
}

int main() {
  std::packaged_task<std::expected<Reply, FetchError>(std::string)> task(
      [](std::string replica) { return try_fetch(replica); });

  auto fut = task.get_future();
  std::jthread worker(std::move(task), "replica-A");

  auto outcome = fut.get(); // throws only on real machinery failure
  if (outcome) {
    std::cout << "got reply: status=" << outcome->status << " body=\""
              << outcome->body << "\"\n";
  } else {
    std::cerr << "domain error: " << to_string(outcome.error()) << "\n";
  }
}
