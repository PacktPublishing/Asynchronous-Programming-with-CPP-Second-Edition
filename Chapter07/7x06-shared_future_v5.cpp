// 7x06-shared_future_v5.cpp
// Running example v5: share() turns a single-consumer future into a
// shared_future that several subscriber threads can read concurrently.
// Note shared_future<T>::get() returns const T& — the value lives in the
// shared state and is read-only to all subscribers.

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
#define sync_cout std::osyncstream(std::cout)

struct Reply {
  int status;
  std::string body;
};

Reply fetch_blocking(const std::string& replica) {
  std::this_thread::sleep_for(200ms);
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

  std::shared_future<Reply> shared = fut.share();   // fut is now invalid

  std::vector<std::jthread> subscribers;
  for (int i = 0; i < 3; ++i) {
    subscribers.emplace_back([shared, i] {
      try {
        const Reply& r = shared.get();              // returns by const reference
        sync_cout << "subscriber " << i
                  << " saw " << r.body.size() << " bytes\n";
      } catch (const std::exception& e) {
        sync_cout << "subscriber " << i
                  << " saw error: " << e.what() << '\n';
      }
    });
  }
}
