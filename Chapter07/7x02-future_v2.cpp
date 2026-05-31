// 7x02-future_v2.cpp
// Running example v2: consumer side. The caller blocks in fut.get() until the
// worker calls set_value, then moves the result out. After get() the future is
// consumed (valid() returns false).

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

Reply fetch_blocking(const std::string& replica) {
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

  Reply r = fut.get();   // blocks until set_value
  std::cout << "got reply: status=" << r.status
            << " body.size=" << r.body.size() << "\n";
  std::cout << "fut.valid()=" << std::boolalpha << fut.valid() << "\n";
}
