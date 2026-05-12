add_subdirectory(OrderBook)
add_subdirectory(Metrics)
add_subdirectory(DataSource)
add_subdirectory(MarketDataFeed)

add_library(market-engine STATIC Dummy.cpp)
target_link_libraries(market-engine
    PRIVATE
    me-order-book
    me-metrics
    me-data-feed
    )

target_include_directories(market-engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)