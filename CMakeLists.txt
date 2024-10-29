cmake_minimum_required(VERSION 3.1)

project(bagconv VERSION 1.0
                  DESCRIPTION "csv-like storage to sqlite"
                  LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "-Wall -g -Wextra -O3")
include_directories(ext/pugixml-1.13/src/ ext/cpp-httplib-0.18.1/ ext/nlohmann/)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(bagconv bagconv.cc sqlwriter.cc rd2wgs84.cc ext/pugixml-1.13/src/pugixml.cpp)
target_link_libraries(bagconv pthread  dl sqlite3 )


add_executable(bagserv bagserv.cc sqlwriter.cc rd2wgs84.cc ext/pugixml-1.13/src/pugixml.cpp jsonhelper.cc)
target_link_libraries(bagserv pthread dl sqlite3 z )

