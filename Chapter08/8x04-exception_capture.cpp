// 8x04-exception_capture.cpp
// ==========================
// std::async wraps the callable so that any exception it throws is stored in
// the shared state automatically. The consumer's get() either returns the
// value or rethrows. No producer-side try/catch/set_exception is needed.

#include <format>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

struct Record {
    int id;
    std::string payload;
};

Record load_record(int id) {
    if (id < 0) throw std::runtime_error{"negative id"};
    return Record{id, "payload-" + std::to_string(id)};
}

void try_load(int id) {
    auto fut = std::async(std::launch::async, load_record, id);
    try {
        Record r = fut.get();  // either returns or rethrows
        std::cout << "loaded id=" << r.id << " payload=\"" << r.payload << "\"\n";
    } catch (const std::runtime_error& e) {
        std::cerr << "load failed: " << e.what() << "\n";
    }
}

// The chapter's form: a lambda that may throw, captured by std::async.
bool some_precondition_failed() { return true; }  // fails, to show the error path
Record load_record() { return Record{7, "payload-7"}; }
void process(const Record& r) { std::cout << "loaded id=" << r.id << " payload=\"" << r.payload << "\"\n"; }
template <typename... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << "\n";
}

void load_with_lambda() {
    auto fut = std::async(std::launch::async, [] {
        if (some_precondition_failed()) throw std::runtime_error{"precondition failed"};
        return load_record();
    });

    try {
        Record r = fut.get();  // either returns or rethrows
        process(r);
    } catch (const std::runtime_error& e) {
        log_error("load failed: {}", e.what());
    }
}

int main() {
    try_load(42);  // success path
    try_load(-1);  // exception path: rethrown by fut.get()
    load_with_lambda();
}
