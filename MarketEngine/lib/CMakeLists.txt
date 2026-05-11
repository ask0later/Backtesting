add_subdirectory(OrderBook)
add_subdirectory(Metrics)
add_subdirectory(DataSource)
add_subdirectory(MarketDataFeed)

add_library(market-engine INTERFACE)

target_link_libraries(market-engine
    INTERFACE
        me-order-book
        me-metrics
        me-data-feed
        )