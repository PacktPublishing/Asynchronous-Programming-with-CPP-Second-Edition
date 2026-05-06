// 3x20-timer.cpp
// ==============
// Putting it all together: a production-quality periodic Timer.
// Uses std::jthread + stop_token for RAII lifetime and cooperative cancellation.
// Notice what is absent: no explicit join(), no atomic<bool> running flag.
//
// Requires: C++20

#include <chrono>
#include <functional>
#include <iostream>
#include <syncstream>
#include <thread>

using namespace std::chrono_literals;
using namespace std::chrono;
#define sync_cout std::osyncstream(std::cout)

template<typename Duration>
class Timer {
public:
    using Callback = std::function<void()>;

    Timer(Duration interval, Callback cb)
        : t_([interval, cb = std::move(cb)](std::stop_token st) {
              sync_cout << "Timer: starting with interval of "
                        << duration_cast<milliseconds>(interval) << "\n";
              int tick = 0;
              while (!st.stop_requested()) {
                  sync_cout << "Timer: tick " << tick++ << "\n";
                  cb();
                  sync_cout << "Timer: sleeping...\n";
                  std::this_thread::sleep_for(interval);
              }
              sync_cout << "Timer: exit\n";
          })
    {}

    void stop() { t_.request_stop(); }
    // Destructor: jthread joins automatically

private:
    std::jthread t_;
};

int main() {
    sync_cout << "Main: creating timer\n";

    Timer timer(1s, []{ sync_cout << "Callback: running\n"; });

    std::this_thread::sleep_for(3500ms);

    sync_cout << "Main: stopping timer\n";
    timer.stop();

    std::this_thread::sleep_for(500ms);
    sync_cout << "Main: exiting\n";
}
