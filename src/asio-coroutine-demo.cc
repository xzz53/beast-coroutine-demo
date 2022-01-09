#include <boost/algorithm/string/split.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/system/detail/error_code.hpp>
#if defined(__clang__)
#include <experimental/coroutine>
#elif defined(__GNUC__)
#include <coroutine>
#endif

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>

#include "my_result.hh"
#include "url.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace this_coro = boost::asio::this_coro;
namespace websocket = beast::websocket;

using boost::system::error_code;
using net::experimental::channel;
using net::ip::tcp;

template <typename T>
using StringResult = Result<T, std::string>;

using result_channel = channel<void(boost::system::error_code, StringResult<std::string>)>;
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

net::awaitable<StringResult<std::string>>
http_get(const std::string url_string) {
    const int version = 11;
    beast::error_code ec;

    // These objects perform our I/O
    tcp::resolver resolver(co_await this_coro::executor);
    beast::tcp_stream stream(co_await this_coro::executor);

    try {
        Url url {url_string};
        const auto host = url.host();
        const auto port = url.port();
        const auto target = url.path();
        const auto scheme = url.scheme();

        if(scheme != "http") {
            co_return Err {"scheme not supported: '" + scheme + "'"};
        }

        // Look up the domain name
        auto results = co_await resolver.async_resolve(host, port, net::use_awaitable);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        co_await stream.async_connect(results, net::use_awaitable);

        // Set up an HTTP GET request message
        http::request<http::string_body> req {http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Set the timeout.
        stream.expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        co_await http::async_write(stream, req, net::use_awaitable);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer b;

        // Declare a container to hold the response
        http::response<http::dynamic_body> res;

        // Receive the HTTP response
        co_await http::async_read(stream, b, res, net::use_awaitable);

        // Write the message to standard out
        // std::cout << res << std::endl;

        // Gracefully close the socket
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        co_return Ok {beast::buffers_to_string(res.body().data())};
    } catch(const std::exception &e) {
        std::cout << "http_get got exception: " << e.what() << std::endl;
        co_return Err {std::string {e.what()}};
    }
}

net::awaitable<void>
http_get_wrapper(const std::string url_string, result_channel &chan) {
    const auto result = co_await http_get(url_string);
    co_await chan.async_send(error_code {}, result, net::use_awaitable);
}

net::awaitable<std::vector<StringResult<std::string>>>
http_get_multiple(const std::vector<std::string> urls) {
    const auto N = urls.size();
    auto ioc = co_await this_coro::executor;

    result_channel chan {ioc};

    for(const auto &url : urls) {
        std::cout << "HTTP requesting " << url << std::endl;
        net::co_spawn(ioc, http_get_wrapper(url, chan), net::detached);
    }

    std::vector<StringResult<std::string>> results;

    // TODO: order
    for(size_t i = 0; i < N; ++i) {
        const auto r = co_await chan.async_receive(net::use_awaitable);
        if(r.is_ok()) {
            std::cout << "HTTP got reply " << *r.ok() << std::endl;
        } else {
            std::cout << "HTTP got error " << *r.err() << std::endl;
        }

        results.push_back(r);
    }

    co_return results;
}

// websocket client session
net::awaitable<void>
websocket_client(websocket::stream<beast::tcp_stream> ws) {
    beast::error_code ec;

    // Set suggested timeout settings for the websocket
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws.set_option(websocket::stream_base::decorator([](websocket::response_type &res) {
        res.set(http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-coro");
    }));

    // Accept the websocket handshake
    co_await ws.async_accept(net::use_awaitable);

    try {
        for(;;) {
            // This buffer will hold the incoming message
            beast::flat_buffer buffer;

            // Read a message
            co_await ws.async_read(buffer, net::use_awaitable);
            auto line = beast::buffers_to_string(buffer.data());

            // Parse URLs
            boost::trim(line);
            std::vector<std::string> urls;
            boost::split(urls, line, boost::is_any_of("\t\r\n "), boost::token_compress_on);

            // Fetch URLs
            const auto result = co_await http_get_multiple(urls);
            std::string result_string;

            for(const auto &r : result) {
                if(r.is_ok()) {
                    result_string += "Ok(" + *r.ok() + ")\n";
                } else {
                    result_string += "Err(" + *r.err() + ")\n";
                }
            }

            // Send the results back
            ws.text(ws.got_text());
            co_await ws.async_write(net::buffer(result_string), net::use_awaitable);
        }
    } catch(const boost::system::system_error &e) {
        if(e.code() != websocket::error::closed) {
            std::cout << "websocket_client got exception2: " << ec << std::endl;
        } else {
            std::cout << "websocket client disconnected" << std::endl;
        }
    } catch(const std::exception &e) {
        std::cout << "websocket_client got exception3: " << e.what() << std::endl;
        // This indicates that the session was closed
        // if(ec == websocket::error::closed)
        //     break;
    }
}

// Accepts incoming connections and launches the sessions
net::awaitable<void>
websocket_listen(tcp::endpoint endpoint) {
    auto ioc = co_await this_coro::executor;
    beast::error_code ec;

    // Open the acceptor
    tcp::acceptor acceptor(ioc);
    acceptor.open(endpoint.protocol(), ec);
    if(ec) {
        std::cout << ec << std::endl;
        co_return;
    }

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        std::cout << ec << std::endl;
        co_return;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if(ec) {
        std::cout << ec << std::endl;
        co_return;
    }

    // Start listening for connections
    acceptor.listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        std::cout << ec << std::endl;
        co_return;
    }

    std::cout << "listening on ws://" << endpoint << std::endl;
    for(;;) {
        tcp::socket socket(ioc);
        co_await acceptor.async_accept(socket, net::use_awaitable);
        std::cout << "websocket client connected" << std::endl;
        net::co_spawn(
            ioc, websocket_client(websocket::stream<beast::tcp_stream>(std::move(socket))),
            net::detached);
    }
}

net::awaitable<void>
test3() {
    const std::vector<std::string> urls {"http://localhost:8081/2", "http://localhost:8081/3",
                                         "http://localhost:8081/4"};
    const auto result = co_await http_get_multiple(urls);

    for(const auto &r : result) {
        if(r.is_ok()) {
            std::cout << *r.ok() << std::endl;
        } else {
            std::cout << *r.err() << std::endl;
        }
    }
}

int
main(int argc, char **argv) {
    net::io_context ioc;

    // net::co_spawn(ioc, http_get("http://localhost:8081/2"), net::detached);
    // net::co_spawn(ioc, test3(), net::detached);

    auto const address = net::ip::make_address("127.0.0.1");
    auto const port = static_cast<unsigned short>(8082);

    net::co_spawn(ioc, websocket_listen(tcp::endpoint {address, port}), net::detached);

    // Run the I/O service.
    ioc.run();

    return EXIT_SUCCESS;
}
