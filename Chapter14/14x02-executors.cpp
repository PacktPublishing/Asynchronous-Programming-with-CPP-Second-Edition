// 14x02-executors.cpp
// ===================
// Asio's executors are the shipped form of the P0443 executor model: a
// lightweight handle onto an execution context, tuned through properties
// with require(), prefer() and query(). This is the vocabulary that
// Chapter 12's scheduler replaced, still in daily use.
//
// Requires Boost 1.74+ (unified executors).
//   g++ -std=c++23 -I<boost>/include 14x02-executors.cpp -o 14x02-executors

#include <boost/asio.hpp>
#include <chrono>
#include <print>
#include <thread>

namespace asio = boost::asio;

int main() {
    asio::io_context io;
    std::jthread runner;
    {
        // [1] The concrete executor can require every property the context
        //     implements. require() returns a new executor type, because
        //     outstanding-work tracking is part of the type, so exec_a is auto.
        asio::io_context::executor_type exec_concrete = io.get_executor();
        auto exec_a = asio::require(exec_concrete, asio::execution::outstanding_work.tracked);

        // [2] The type-erased executor can require only blocking.never. The
        //     other properties are hints, passed through prefer().
        asio::any_io_executor exec_erased = exec_concrete;
        asio::any_io_executor exec_b = asio::require(exec_erased, asio::execution::blocking.never);
        asio::any_io_executor exec_c = asio::prefer(exec_erased, asio::execution::outstanding_work.tracked);

        // The following line does not compile as outstanding_work.tracked
        // cannot be required on an executor of type any_io_executor
        // asio::require(exec_erased, asio::execution::outstanding_work.tracked);

        // [3] Querying a property
        bool tracked = asio::query(exec_a, asio::execution::outstanding_work) ==
                       asio::execution::outstanding_work.tracked;  // true
        std::println("exec_a tracked: {}", tracked);
        std::println("exec_b blocking.never: {}",
                     asio::query(exec_b, asio::execution::blocking) == asio::execution::blocking.never);

        // [4] execute() submits a function; post() and dispatch() are the same
        //     thing with a chosen blocking property.
        exec_b.execute([] { std::println("executed on the context"); });
        asio::post(exec_erased, [] { std::println("posted to the context"); });
        asio::dispatch(exec_erased, [] { std::println("dispatched (may run inline)"); });

        // [5] A tracked executor keeps run() alive; drop it and run() can return.
        runner = std::jthread([&io] {
            io.run();
            std::println("run() returned");
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::println("releasing the tracked executors");
    }  // exec_a and exec_c are destroyed here: the last work is gone
}
