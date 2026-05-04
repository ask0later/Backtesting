add_library(me-order-book BookEvent.cpp)

target_include_directories(me-order-book PUBLIC
    ${PROJECT_SOURCE_DIR}/include)