#include <boost/beast/http/status.hpp>
#include <unistd.h>
#if defined(__clang__)
#include <experimental/coroutine>
#elif defined(__GNUC__)
#include <coroutine>
#endif

#include <concepts>
#include <cstdlib>
#include <exception>
#include <regex>
#include <string>

#include <boost/algorithm/string.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include "spdlog/fmt/chrono.h"
#include "spdlog/fmt/ostr.h"
#include "spdlog/spdlog.h"

#include "my_result.hh"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace this_coro = boost::asio::this_coro;
namespace logging = spdlog;

using net::ip::tcp;

// Get ISO 8601 - like string representation of current date and time
// with millisecond precision
std::string
current_time_string() {
    std::string result;
    char buffer[26];
    int millisec;
    struct tm tm_info;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    millisec = lrint(tv.tv_usec / 1000.0); // Round to nearest millisec
    if(millisec >= 1000) {                 // Allow for rounding up to nearest second
        millisec -= 1000;
        tv.tv_sec++;
    }

    localtime_r(&tv.tv_sec, &tm_info);

    strftime(buffer, 26, "%F %T", &tm_info);
    return fmt::format("{}.{:03d}", buffer, millisec);
}

// Emulate CPU-intensive request handler
net::awaitable<std::string>
background_job(float delay) {
    const auto t1 = current_time_string();
    logging::info("background job starts, delay={}", delay);
    ::usleep(1e6 * delay);
    const auto t2 = current_time_string();
    logging::info("background job ends, delay={}", delay);
    co_return fmt::format("Slept {:.3f} s from {} to {}", delay, t1, t2);
}

// This function produces an HTTP response for the given
// request.
template <class Body, class Allocator>
net::awaitable<void>
handle_request(net::thread_pool &work_pool, beast::tcp_stream &stream,
               http::request<Body, http::basic_fields<Allocator>> &&req) {
    const auto target = req.target().to_string();
    std::smatch sm;

    http::response<http::string_body> res;

    res.version(req.version());
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");

    // Make sure we can handle the method
    if(req.method() != http::verb::get) {
        res.result(http::status::bad_request);
        res.body() = "Unknown HTTP-method";
        res.prepare_payload();
        logging::error("bad http method: {}", req.method());
        goto send;
    }

    if(std::regex_match(target, sm, std::regex {"/((\\d+\\.)?\\d+)"})) {
        const auto delay = std::stof(sm[1]);
        logging::info("scheduling background job, delay={}", delay);
        res.result(http::status::ok);

        // Offload CPU-intensive processing to a separate thread pool.
        res.body() =
            co_await net::co_spawn(work_pool, background_job(delay), net::use_awaitable);
        res.prepare_payload();
    } else {
        res.result(http::status::not_found);
        res.body() = "Not found\n";
        res.prepare_payload();
    }

send:
    logging::info("sending http response, status={}", res.result());
    co_await http::async_write(stream, res, net::use_awaitable);
    co_return;
}

//------------------------------------------------------------------------------

// Handles an HTTP server connection
net::awaitable<void>
http_client(net::thread_pool &work_pool, beast::tcp_stream stream) {
    bool close = false;
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    // send_lambda lambda {stream, close, ec, yield};

    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));

    try {
        // Read a request
        http::request<http::string_body> req;
        co_await http::async_read(stream, buffer, req, net::use_awaitable);
        logging::info("request location '{}'", req.target());
        // Send the response
        co_await handle_request(work_pool, stream, std::move(req));
    } catch(const std::exception &e) {
        logging::error("http_client got exception {}", e.what());
    }

    // Send a TCP shutdown
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
net::awaitable<void>
http_listen(net::thread_pool &work_pool, tcp::endpoint endpoint) {
    auto ioc = co_await this_coro::executor;
    beast::error_code ec;

    // Open the acceptor
    tcp::acceptor acceptor(ioc);
    acceptor.open(endpoint.protocol(), ec);
    if(ec) {
        logging::error("open: {}", ec.what());
        co_return;
    }

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        logging::error("set_options: {}", ec.what());
        co_return;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if(ec) {
        logging::error("bind: {}", ec.what());
        co_return;
    }

    // Start listening for connections
    acceptor.listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        logging::error("listen {}", ec.what());
        co_return;
    }

    logging::info("listening on http://{}", endpoint);
    for(;;) {
        tcp::socket socket {ioc};
        co_await acceptor.async_accept(socket, net::use_awaitable);
        logging::info("http request from {}", socket.remote_endpoint());
        net::co_spawn(ioc, http_client(work_pool, beast::tcp_stream {std::move(socket)}),
                      net::detached);
    }
}

int
main(int, char **) {
    const size_t n_threads = 2;
    net::io_context ioc;
    net::thread_pool work_pool {n_threads};

    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(8081);

    logging::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$] %v");

    net::co_spawn(ioc, http_listen(work_pool, tcp::endpoint {address, port}), net::detached);

    ioc.run();
    return EXIT_SUCCESS;
}
