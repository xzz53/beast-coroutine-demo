cmake_minimum_required(VERSION 3.11)
project(asio-coroutine-demo VERSION 0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(Threads)

add_executable(asio-coroutine-demo
  src/asio-coroutine-demo.cc
  thirdparty/CxxUrl/url.cpp)

target_compile_definitions(asio-coroutine-demo PRIVATE
  FMT_HEADER_ONLY)

target_include_directories(asio-coroutine-demo PRIVATE
  thirdparty/)

target_link_libraries(asio-coroutine-demo PRIVATE
  Boost::headers
  Threads::Threads)

target_compile_options(asio-coroutine-demo PRIVATE
  -fcoroutines)
