cmake_minimum_required(VERSION 3.27)
project(MarketEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib)

message(STATUS "Testing configuration")
find_package(GTest CONFIG REQUIRED)
enable_testing()
add_subdirectory(tests)
