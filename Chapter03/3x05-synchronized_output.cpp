// 3x05-synchronized_output.cpp
// ============================
// Three approaches to console output from multiple threads:
//   1. Raw std::cout          — interleaved output (intentionally broken demo)
//   2. std::ostringstream     — pre-C++20 single-write workaround
//   3. std::osyncstream       — C++20 canonical solution
//
// Requires: C++20

#include <iostream>
#include <sstream>
#include <syncstream>
#include <thread>

#define sync_cout std::osyncstream(std::cout)

// Demo 1: raw std::cout — individual << calls are thread-safe since C++14,
// but adjacent calls are not atomic, producing interleaved output.
void demo_raw_cout() {
    std::thread t1([]{ for (int i = 0; i < 5; ++i)
                           std::cout << "1 " << "2 " << "3 " << "4 \n"; });
    std::thread t2([]{ for (int i = 0; i < 5; ++i)
                           std::cout << "5 " << "6 " << "7 " << "8 \n"; });
    t1.join(); t2.join();
}

// Demo 2: ostringstream workaround (pre-C++20) — format into a local buffer,
// then flush with a single atomic write.
void demo_ostringstream() {
    std::thread t1([]{ for (int i = 0; i < 5; ++i) {
                           std::ostringstream oss;
                           oss << "1 " << "2 " << "3 " << "4 \n";
                           std::cout << oss.str();  // single atomic write
                       }});
    std::thread t2([]{ for (int i = 0; i < 5; ++i) {
                           std::ostringstream oss;
                           oss << "5 " << "6 " << "7 " << "8 \n";
                           std::cout << oss.str();
                       }});
    t1.join(); t2.join();
}

// Demo 3: std::osyncstream (C++20) — buffers internally, transfers atomically
// on destruction; no interleaving between threads.
void demo_osyncstream() {
    std::thread t1([]{ for (int i = 0; i < 5; ++i)
                           sync_cout << "1 " << "2 " << "3 " << "4 \n"; });
    std::thread t2([]{ for (int i = 0; i < 5; ++i)
                           sync_cout << "5 " << "6 " << "7 " << "8 \n"; });
    t1.join(); t2.join();
}

int main() {
    std::cout << "--- raw std::cout (may interleave) ---\n";
    demo_raw_cout();

    std::cout << "\n--- ostringstream workaround ---\n";
    demo_ostringstream();

    std::cout << "\n--- std::osyncstream (C++20) ---\n";
    demo_osyncstream();
}
