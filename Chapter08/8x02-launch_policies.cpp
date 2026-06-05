// 8x02-launch_policies.cpp
// Demonstrates the three forms: std::launch::async, std::launch::deferred,
// and the implementation-defined default. Shows the is_deferred detection
// idiom using wait_for(0s).

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

using namespace std::chrono_literals;

int square(const std::string& task_name, int x) {
  sync_cout << "Launching " << task_name << "...\n";
  return x * x;
}

int main() {
  sync_cout << "Starting main thread...\n";

  auto fut_async    = std::async(std::launch::async,
                                 square, "async_policy", 2);
  auto fut_deferred = std::async(std::launch::deferred,
                                 square, "deferred_policy", 3);
  auto fut_default  = std::async(square,
                                 "default_policy", 4);

  // Detection idiom. wait_for(0s) returns deferred for a deferred task
  // without triggering its execution.
  auto is_deferred = [](const std::future<int>& fut) {
    return fut.wait_for(0s) == std::future_status::deferred;
  };

  sync_cout << "Checking if deferred:\n"
            << "  fut_async    = " << std::boolalpha
            << is_deferred(fut_async) << '\n'
            << "  fut_deferred = " << std::boolalpha
            << is_deferred(fut_deferred) << '\n'
            << "  fut_default  = " << std::boolalpha
            << is_deferred(fut_default) << '\n';

  sync_cout << "Sleeping 1s on the main thread...\n";
  std::this_thread::sleep_for(1s);

  // get() on the deferred future is what triggers its body to run, here,
  // on the main thread.
  sync_cout << "Getting results:\n"
            << "  fut_async    = " << fut_async.get() << '\n'
            << "  fut_deferred = " << fut_deferred.get() << '\n'
            << "  fut_default  = " << fut_default.get() << '\n';
}
