cmake_minimum_required(VERSION 3.10)
project(wasapi_loopback_recorder)

set(CMAKE_CXX_STANDARD 17)

add_executable(wasapi_loopback_recorder wasapi_loopback_recorder.cpp)

target_link_libraries(wasapi_loopback_recorder ole32 uuid winmm)
