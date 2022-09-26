cmake_minimum_required(VERSION 3.0.0)
project(cdexcom VERSION 0.1.0)

find_package(CURL REQUIRED)
link_libraries(${CURL_LIBRARIES})
include_directories(${CURL_INCLUDE_DIR})
add_executable(cdexcom main.cpp cdexcom.h json.h)

