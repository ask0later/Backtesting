function(add_test_executable TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    target_link_libraries(${TEST_NAME} 
        PRIVATE 
            gtest::gtest
            ${ARGN}
    )
    
    target_include_directories(${TEST_NAME} 
        PRIVATE 
            ${GMOCK_INCLUDE_DIRS} 
            ${GTEST_INCLUDE_DIRS}
    )
    
    add_test(
        NAME ${TEST_NAME}
        COMMAND $<TARGET_FILE:${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )
endfunction()

# Order book
add_test_executable(order-book-test OrderBookTests.cpp me-order-book)

# Csv Table
add_test_executable(csv-table-test CsvTableTests.cpp me-csv-table)

# Data Feed
add_test_executable(data-feed-test MarketDataFeedTests.cpp me-data-feed)

# Engine
add_test_executable(engine-test EngineTests.cpp me-data-feed me-order-book)

# Replay
add_test_executable(replay-test ReplayTests.cpp me-data-feed me-order-book)
