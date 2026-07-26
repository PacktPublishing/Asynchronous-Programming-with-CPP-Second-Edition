// 7x11-no_composition.cpp
// =======================
// Demonstrates the "no .then()" limitation: chaining fetch -> parse -> validate
// using only std::future requires nested wait/get and manual exception relay.
// Compare with the senders version in 7x17-senders_dag.cpp.

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

struct Raw {
  std::string bytes;
};
struct Parsed {
  int field;
};

Raw fetch(const std::string &replica) {
  std::this_thread::sleep_for(50ms);
  return Raw{"raw from " + replica};
}
Parsed parse(Raw r) {
  std::this_thread::sleep_for(20ms);
  return Parsed{static_cast<int>(r.bytes.size())};
}
bool validate(Parsed p) {
  std::this_thread::sleep_for(10ms);
  return p.field > 0;
}

int main() {
  // Stage 1
  std::packaged_task<Raw(std::string)> fetch_task(fetch);
  auto fetch_fut = fetch_task.get_future();
  std::jthread t1(std::move(fetch_task), "replica-A");

  Raw raw = fetch_fut.get(); // blocks

  // Stage 2 — re-launch a worker for the next step.
  std::packaged_task<Parsed(Raw)> parse_task(parse);
  auto parse_fut = parse_task.get_future();
  std::jthread t2(std::move(parse_task), std::move(raw));

  Parsed parsed = parse_fut.get(); // blocks again

  // Stage 3
  std::packaged_task<bool(Parsed)> validate_task(validate);
  auto validate_fut = validate_task.get_future();
  std::jthread t3(std::move(validate_task), std::move(parsed));

  bool ok = validate_fut.get(); // blocks again
  std::cout << "valid=" << std::boolalpha << ok << "\n";

  // What we want to write: fetch | parse | validate.
  // What we have to write: the above. See Chapter 13 for the senders form.
}
