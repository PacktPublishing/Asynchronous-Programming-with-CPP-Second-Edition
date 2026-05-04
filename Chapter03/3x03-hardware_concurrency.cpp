// 3x03-hardware_concurrency.cpp
// =============================
// Querying the number of logical CPU cores with hardware_concurrency().
// Returns a hint that may be 0 when undetermined — always guard against it.
//
// Requires: C++11

#include <iostream>
#include <thread>

int main() {
    const auto cores = std::thread::hardware_concurrency();
    std::cout << "Logical cores reported: " << cores << "\n";

    // Returns 0 if the value cannot be determined — always guard against this
    const std::size_t n_threads = (cores > 0) ? cores : 4;
    std::cout << "Threads to spawn: " << n_threads << "\n";
}
