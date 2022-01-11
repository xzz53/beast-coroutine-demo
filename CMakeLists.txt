cmake_minimum_required(VERSION 3.11)
project(beast-coroutine-demo VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(Threads)

add_executable(websocket-proxy
  src/websocket-proxy.cc
  thirdparty/CxxUrl/url.cpp)

target_compile_definitions(websocket-proxy PRIVATE
  FMT_HEADER_ONLY)

target_include_directories(websocket-proxy PRIVATE
  thirdparty/)

target_link_libraries(websocket-proxy PRIVATE
  Boost::headers
  Threads::Threads)

target_compile_options(websocket-proxy PRIVATE
  -fcoroutines)
