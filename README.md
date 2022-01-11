# Boost Beast HTTP/Websocket C++ 20 coroutine demo app

## Build dependencies (older versions may be compatible, but have not been tested against)
* gcc 11.1, cmake
* boost 1.78
* python >=3.6, [aiohttp](https://pypi.org/project/aiohttp/) 3.8.1 (for `sleepy-server.py`)
* [websocat](https://github.com/vi/websocat) (for testing)

## Building

```shell
mkdir build && cmake -B build && cmake --build build
```

## Running
The basic operation mode of the demo server is to receive space
separated lists of URLs from multiple clients via websocket, fetch
them concurrently via HTTP and return results to the clients as soon
as all HTTP requests are completed. At this time the fetcher is rather
simplistic: it ignores redirects and does not support https. Included
test http server (`sleepy-server.py`) serves URLs like
`http://localhost:8081/delay`, where `delay` is a real number, by
sleeping for `delay` seconds and returning a single-line response. In
order to start the demo, run the following commands.

In shell 1:
```shell
./sleepy-server.py
```

In shell 2:
```shell
./build/websocket-proxy
```

Then, in shell 3, try sending requests like
```shell
echo http://localhost:8081/2 http://localhost:8081/3 http://localhost:8081/4 | websocat ws://127.0.0.1:8082
```

Concurrent requests are supported, you can try it out by running
multiple `websocat`s in parallel.
