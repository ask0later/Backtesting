cmake_minimum_required(VERSION 3.27)
project(MarketEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(rapidcsv REQUIRED)
add_subdirectory(lib)

option(MARKET_ENGINE_BUILD_TESTS "Build MarketEngine tests" ON)
if (MARKET_ENGINE_BUILD_TESTS)
  message(STATUS "Testing configuration")
  find_package(GTest CONFIG REQUIRED)
  enable_testing()
  add_subdirectory(tests)
endif()