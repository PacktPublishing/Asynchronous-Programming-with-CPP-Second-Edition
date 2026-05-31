// 7x07-packaged_task_intro.cpp
// std::packaged_task fundamentals: wrap a callable, retrieve its future,
// invoke it on a thread or synchronously.

#include <cmath>
#include <future>
#include <iostream>
#include <thread>

int main() {
  // Lambda dispatched to a worker thread.
  std::packaged_task<double(double, double)> task1(
      [](double a, double b) { return std::pow(a, b); });
  auto fut1 = task1.get_future();
  std::jthread t(std::move(task1), 2.0, 10.0);
  std::cout << "task1: 2^10 = " << fut1.get() << "\n";

  // Lambda invoked synchronously.
  std::packaged_task<double(double, double)> task2([](double a, double b) {
    return std::pow(a, b);
  });
  auto fut2 = task2.get_future();
  task2(3.0, 4.0);     // invoke directly — no thread
  std::cout << "task2: 3^4 = " << fut2.get() << "\n";
}
