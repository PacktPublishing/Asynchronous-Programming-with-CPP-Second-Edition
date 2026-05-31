// 7x14-promise_setter.cpp
// Fix for the jthread x promise pitfall (see 7x13): an RAII guard that
// converts a missed set_value/set_exception into an eager broken_promise so
// the consumer learns about the failure as soon as the worker's stack unwinds.

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

struct Reply { int status; };

template <typename T>
class promise_setter {
  std::promise<T>& prom_;
  bool armed_ = true;

public:
  explicit promise_setter(std::promise<T>& p) : prom_(p) {}

  promise_setter(const promise_setter&) = delete;
  promise_setter& operator=(const promise_setter&) = delete;

  ~promise_setter() {
    if (armed_) {
      try {
        prom_.set_exception(std::make_exception_ptr(
            std::future_error{std::future_errc::broken_promise}));
      } catch (...) {
        // promise already dead — nothing we can do.
      }
    }
  }

  void set(T v) {
    prom_.set_value(std::move(v));
    armed_ = false;
  }
  void set_exception(std::exception_ptr e) {
    prom_.set_exception(std::move(e));
    armed_ = false;
  }
};

int main() {
  bool some_precondition_failed = true;

  std::promise<Reply> prom;
  auto fut = prom.get_future();

  auto t0 = std::chrono::steady_clock::now();

  std::jthread worker([&prom, some_precondition_failed]() mutable {
    promise_setter<Reply> guard(prom);   // arms the safety net
    std::this_thread::sleep_for(500ms);  // setup
    if (some_precondition_failed) {
      std::cerr << "[worker] precondition failed; returning early\n";
      return;   // guard's destructor relays broken_promise immediately
    }
    guard.set(Reply{200});
  });

  try {
    Reply r = fut.get();
    std::cout << "status=" << r.status << "\n";
  } catch (const std::future_error& e) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    std::cerr << "broken_promise after " << elapsed << " ms (still ~500 ms in this demo)\n";
    std::cerr << "but the worker's exit path is now deterministic, not racy.\n";
  }
}
