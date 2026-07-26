// 12x06-transfer_between_schedulers.cpp
// =====================================
// starts_on sets a pipeline's entry resource.
// continues_on moves the continuation onto another.
// Here an "I/O" run_loop and a CPU pool hand work back and forth:
// parse on I/O, compute on CPU, respond on I/O.
//
// Requires C++26 std::execution (P2300) NVIDIA stdexec reference
// implementation (https://github.com/NVIDIA/stdexec).
// No shipping standard library implements <execution> senders yet.
//   g++ -std=c++26 -I<path-to-stdexec>/include
//       12x06-transfer_between_schedulers.cpp
//       -o 12x06-transfer_between_schedulers

#include <print>
#include <string>
#include <thread>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

namespace ex = stdexec;

int parse(std::string s) {
    return static_cast<int>(s.size());
}

int compute(int n) {
    return n * n;
}

void write_response(int r) {
    std::println("Response = {}", r);
}

int main() {
  ex::run_loop io_loop;
  std::jthread io_driver([&io_loop] {
    io_loop.run();
  });
  ex::scheduler auto io_sched = io_loop.get_scheduler();

  exec::static_thread_pool cpu{4};
  ex::scheduler auto cpu_sched = cpu.get_scheduler();

  ex::sender auto pipeline =
      ex::starts_on(io_sched, ex::just(std::string{"request-body"})) |
      ex::then(parse)               // On the I/O thread
    | ex::continues_on(cpu_sched)   // Hop to the CPU pool
    | ex::then(compute)             // On the CPU pool
    | ex::continues_on(io_sched)    // Hop back to I/O
    | ex::then(write_response);     // On the I/O thread

  ex::sync_wait(std::move(pipeline));
  io_loop.finish();
}
