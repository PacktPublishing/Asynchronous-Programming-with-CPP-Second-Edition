// 14x09-strands.cpp
// =================
// Running one io_context from several threads makes handlers concurrent.
// Without serialization a shared counter races; through a strand the same
// handlers never overlap, with no mutex in sight.
//
//   g++ -std=c++23 -I<boost>/include 14x09-strands.cpp

#include <boost/asio.hpp>
#include <print>
#include <thread>
#include <vector>

namespace asio = boost::asio;

// Run the context from every core until it runs out of work, then reset it.
void run_from_every_core(asio::io_context &io) {
    std::vector<std::jthread> threads;
    for (unsigned t = 0; t < std::thread::hardware_concurrency(); ++t) threads.emplace_back([&] { io.run(); });
    threads.clear();  // join all threads
    io.restart();     // allow another run()
}

long count_with(asio::io_context &io, auto executor) {
    long counter = 0;                                                            // deliberately not atomic
    for (int i = 0; i < 200'000; ++i) asio::post(executor, [&] { ++counter; });  // [1]
    run_from_every_core(io);                                                     // [2] N threads call io.run()
    return counter;
}

int main() {
    asio::io_context io;
    std::println("expected      : {}", 200'000);
    std::println("plain executor: {}", count_with(io, io.get_executor()));
    std::println("through strand: {}", count_with(io, asio::make_strand(io)));  // [3]
}
