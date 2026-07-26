// 7x08-packaged_task_v6.cpp
// =========================
// Running example v6: refactor the fetcher to use std::packaged_task. The
// worker-side try/catch boilerplate from v3 is gone. packaged_task wraps the
// callable with that pattern internally.

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

Reply fetch_blocking(const std::string &replica) {
  std::this_thread::sleep_for(200ms);
  return Reply{200, "payload from " + replica};
}

int main() {
  std::packaged_task<Reply(std::string)> task([](std::string replica) -> Reply {
    return fetch_blocking(replica); // exceptions captured automatically
  });

  std::future<Reply> fut = task.get_future();
  std::jthread worker(std::move(task), "replica-A");

  if (fut.wait_for(500ms) == std::future_status::ready) {
    try {
      Reply r = fut.get();
      std::cout << "status=" << r.status << " body=\"" << r.body << "\"\n";
    } catch (const std::system_error &e) {
      std::cerr << "fetch failed: " << e.what() << "\n";
    }
  } else {
    std::cerr << "WARN: replica-A exceeded 500ms budget\n";
  }
}
