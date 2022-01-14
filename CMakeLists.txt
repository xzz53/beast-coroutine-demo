cmake_minimum_required(VERSION 3.11)
project(beast-coroutine-demo VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED COMPONENTS coroutine)
find_package(Threads)

function(add_asio_executable tgt)
  add_executable(${tgt} "${ARGN}")
  target_compile_definitions(${tgt} PRIVATE
    FMT_HEADER_ONLY)

  target_include_directories(${tgt} PRIVATE
    thirdparty/)

  target_link_libraries(${tgt} PRIVATE
    Boost::headers
    Threads::Threads)

  target_compile_options(${tgt} PRIVATE
    -fcoroutines)
endfunction()

# websocket to http proxy
add_asio_executable(websocket-proxy
  src/websocket-proxy.cc
  thirdparty/CxxUrl/url.cpp)

add_asio_executable(sleepy-server
  src/sleepy-server.cc)
