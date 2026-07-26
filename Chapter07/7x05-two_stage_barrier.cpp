// 7x05-two_stage_barrier.cpp
// ==========================
// Two-stage worker using std::promise<void> + std::future<void>::wait() as
// one-shot barriers. A worker reads numbers, signals stage 1, reads letters,
// signals stage 2. The main thread interleaves work between signals.
//
// For new code, prefer std::latch or std::barrier — see 7x10-latch_barrier.cpp.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main() {
  std::istringstream iss_numbers{"10 5 2 6 4 1 3 9 7 8"};
  std::istringstream iss_letters{"A b 53 C,d 83D 4B ca"};
  std::vector<int> numbers;
  std::set<char> letters;

  std::promise<void> numbers_promise, letters_promise;
  auto numbers_ready = numbers_promise.get_future();
  auto letters_ready = letters_promise.get_future();

  std::jthread worker([&] {
    // Stage 1: read numbers.
    std::copy(std::istream_iterator<int>{iss_numbers},
              std::istream_iterator<int>{}, std::back_inserter(numbers));
    numbers_promise.set_value(); // signal stage 1 done

    // Simulate further I/O before stage 2 completes.
    std::this_thread::sleep_for(200ms);

    // Stage 2: read letters.
    std::copy_if(
        std::istreambuf_iterator<char>{iss_letters},
        std::istreambuf_iterator<char>{}, std::inserter(letters, letters.end()),
        [](char c) { return std::isalpha(static_cast<unsigned char>(c)); });
    letters_promise.set_value(); // signal stage 2 done
  });

  numbers_ready.wait();
  std::sort(numbers.begin(), numbers.end());

  if (letters_ready.wait_for(50ms) == std::future_status::timeout) {
    // Letters still not ready — print numbers in the meantime.
    for (int n : numbers)
      std::cout << n << ' ';
    std::cout << "(numbers printed early)\n";
    numbers.clear();
  }
  letters_ready.wait();

  for (int n : numbers)
    std::cout << n << ' ';
  std::cout << '\n';
  for (char c : letters)
    std::cout << c << ' ';
  std::cout << '\n';
}
