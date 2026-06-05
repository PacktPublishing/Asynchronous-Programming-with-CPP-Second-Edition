// 8x04-exception_capture.cpp
// std::async wraps the callable so that any exception it throws is stored in
// the shared state automatically. The consumer's get() either returns the
// value or rethrows. No producer-side try/catch/set_exception is needed.

#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

struct Record {
  int id;
  std::string payload;
};

Record load_record(int id) {
  if (id < 0)
    throw std::runtime_error{"negative id"};
  return Record{id, "payload-" + std::to_string(id)};
}

void try_load(int id) {
  auto fut = std::async(std::launch::async, load_record, id);
  try {
    Record r = fut.get();        // either returns or rethrows
    std::cout << "loaded id=" << r.id
              << " payload=\"" << r.payload << "\"\n";
  } catch (const std::runtime_error& e) {
    std::cerr << "load failed: " << e.what() << "\n";
  }
}

int main() {
  try_load(42);    // success path
  try_load(-1);    // exception path: rethrown by fut.get()
}
