// 7x09-expected_pairing.cpp
// =========================
// Modern pairing: std::expected<Reply, FetchError> as the value channel.
// Domain-level failures travel as values; only true machinery failures throw.
// Requires C++23 (-std=c++23).

#include <chrono>
#include <expected>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

struct Reply {
    int status;
    std::string body;
};

enum class FetchError { NotFound, Timeout, Corrupted };

const char* to_string(FetchError e) {
    switch (e) {
        case FetchError::NotFound:
            return "NotFound";
        case FetchError::Timeout:
            return "Timeout";
        case FetchError::Corrupted:
            return "Corrupted";
    }
    return "?";
}

// Returns std::expected — never throws for expected failures.
std::expected<Reply, FetchError> try_fetch(const std::string& replica) {
    std::this_thread::sleep_for(150ms);
    if (replica == "replica-missing") return std::unexpected(FetchError::NotFound);
    return Reply{200, "payload from " + replica};
}

// What the caller does with each outcome.
void process(const Reply& r) { std::cout << "got reply: status=" << r.status << " body=\"" << r.body << "\"\n"; }
void log_info(const char* what) { std::cout << "info: " << what << "\n"; }
void schedule_retry() { std::cout << "timeout: retry scheduled\n"; }
void alert_oncall() { std::cout << "corrupted reply: on-call alerted\n"; }

void fetch(const std::string& replica) {
    std::packaged_task<std::expected<Reply, FetchError>(std::string)> task(
        [](std::string replica) -> std::expected<Reply, FetchError> {
            auto r = try_fetch(replica);  // returns expected, never throws
            return r;
        });

    auto fut = task.get_future();
    std::jthread worker(std::move(task), replica);

    auto outcome = fut.get();  // throws only on real machinery failure
    if (outcome) {
        process(*outcome);
    } else {
        std::cerr << "domain error: " << to_string(outcome.error()) << "\n";
        switch (outcome.error()) {
            case FetchError::NotFound:
                log_info("404");
                break;
            case FetchError::Timeout:
                schedule_retry();
                break;
            case FetchError::Corrupted:
                alert_oncall();
                break;
        }
    }
}

int main() {
    fetch("replica-A");        // success
    fetch("replica-missing");  // NotFound
}
