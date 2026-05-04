add_executable(order-book-test OrderBookTests.cpp)

target_link_libraries(order-book-test gtest::gtest me-order-book)
target_include_directories(order-book-test PRIVATE ${GMOCK_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})

add_test(
    NAME order-book-test
    COMMAND $<TARGET_FILE:order-book-test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..)