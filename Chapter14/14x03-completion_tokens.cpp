// 14x03-completion_tokens.cpp
// ===========================
// One asynchronous operation, many forms. The completion token passed as
// the last argument decides how the result comes back: a callback, a future,
// an awaitable, a deferred operation, a tuple instead of an exception, a
// deadline, or an operation bound to an executor or a cancellation slot.
//
// Requires Boost 1.86+ (cancel_after, deferred default, partial adapters).
//   g++ -std=c++23 -I<boost>/include 14x03-completion_tokens.cpp

#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <future>
#include <print>
#include <string_view>

namespace asio = boost::asio;
using boost::system::error_code;
using namespace std::chrono_literals;

// A connected pair of local sockets stands in for the network.
struct wire {
    asio::local::stream_protocol::socket reader, writer;
    std::array<char, 64> buf{};
    explicit wire(asio::io_context &io) : reader(io), writer(io) { asio::local::connect_pair(reader, writer); }
    void send(std::string_view s) { asio::write(writer, asio::buffer(s)); }
    std::string_view text(std::size_t n) const { return {buf.data(), n}; }
};

asio::awaitable<void> coroutine_variants(wire &l) {
    // [4] use_awaitable: co_await, exceptions on error.
    l.send("awaitable");
    auto n = co_await l.reader.async_read_some(asio::buffer(l.buf), asio::use_awaitable);
    std::println("use_awaitable : {}", l.text(n));

    // [5] deferred is the default token, so no token at all reads the same.
    l.send("deferred");
    n = co_await l.reader.async_read_some(asio::buffer(l.buf));
    std::println("deferred      : {}", l.text(n));

    // [6] as_tuple: the error is a value, not an exception.
    l.send("as_tuple");
    auto [ec, m] = co_await l.reader.async_read_some(asio::buffer(l.buf), asio::as_tuple);
    std::println("as_tuple      : {} (ec = {})", l.text(m), ec.message());

    // [7] cancel_after: a deadline on this one operation.
    auto [tec, k] = co_await l.reader.async_read_some(asio::buffer(l.buf), asio::cancel_after(50ms, asio::as_tuple));
    std::println("cancel_after  : {} bytes, ec = {}", k, tec.message());
}

int main() {
    asio::io_context io;
    wire l(io);

    // [1] A callback: the classic completion handler.
    l.send("callback");
    l.reader.async_read_some(asio::buffer(l.buf), [&](error_code ec, std::size_t n) {
        std::println("callback      : {} (ec = {})", l.text(n), ec.message());
    });
    io.run();  // drive the loop until that operation completes
    io.restart();

    // [2] use_future: a std::future that another thread would block on.
    l.send("future");
    std::future<std::size_t> f = l.reader.async_read_some(asio::buffer(l.buf), asio::use_future);
    io.run();
    io.restart();
    std::println("use_future    : {}", l.text(f.get()));

    // [3] bind_executor and bind_cancellation_slot decorate any other token.
    asio::cancellation_signal cancel;
    l.reader.async_read_some(asio::buffer(l.buf),
                             asio::bind_cancellation_slot(
                                 cancel.slot(), asio::bind_executor(io.get_executor(), [](error_code ec, std::size_t) {
                                     std::println("bound token   : ec = {}", ec.message());
                                 })));
    cancel.emit(asio::cancellation_type::total);
    io.run();
    io.restart();

    // The coroutine variants, [4] to [7].
    asio::co_spawn(io, coroutine_variants(l), asio::detached);
    io.run();
}
