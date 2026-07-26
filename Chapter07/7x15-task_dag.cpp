// 7x15-task_dag.cpp
// =================
// Asynchronous task DAG built from std::shared_future<void> as completion
// signals and std::jthread as the execution unit per task.
//
// Topology (Figure 7.3):
//   task1 -> task2 -> { task3, task4 } -> task5
//
// Two structural caveats apply: the graph must be acyclic, and you must have
// enough threads to run the maximum-width layer concurrently.

#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
#define sync_cout std::osyncstream(std::cout)

template <typename Func> class Task {
public:
  Task(int id, Func &func) : id_(id), func_(func) {
    fut_ = prom_.get_future().share();
  }
  template <typename... Futures>
  Task(int id, Func &func, Futures &&...deps) : id_(id), func_(func) {
    fut_ = prom_.get_future().share();
    (deps_.push_back(std::forward<Futures>(deps)), ...);
  }

  std::shared_future<void> get_dependency() const { return fut_; }

  void operator()() {
    sync_cout << "task " << id_ << ": waiting for " << deps_.size()
              << " predecessor(s)\n";
    for (auto &d : deps_)
      d.get();
    sync_cout << "task " << id_ << ": running\n";
    func_();
    sync_cout << "task " << id_ << ": signalling completion\n";
    prom_.set_value();
  }

private:
  int id_;
  Func &func_;
  std::promise<void> prom_;
  std::shared_future<void> fut_;
  std::vector<std::shared_future<void>> deps_;
};

int main() {
  auto sleep1s = [] { std::this_thread::sleep_for(1s); };
  auto sleep2s = [] { std::this_thread::sleep_for(2s); };

  Task task1(1, sleep1s);
  Task task2(2, sleep2s, task1.get_dependency());
  Task task3(3, sleep1s, task2.get_dependency());
  Task task4(4, sleep2s, task2.get_dependency());
  Task task5(5, sleep2s, task3.get_dependency(), task4.get_dependency());

  std::jthread t1(std::ref(task1));
  std::jthread t2(std::ref(task2));
  std::jthread t3(std::ref(task3));
  std::jthread t4(std::ref(task4));
  std::jthread t5(std::ref(task5));

  task5.get_dependency().get();
  sync_cout << "All done!\n";
}
