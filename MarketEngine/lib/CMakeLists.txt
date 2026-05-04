add_subdirectory(OrderBook)

add_library(market-engine INTERFACE)

target_link_libraries(market-engine
    INTERFACE
        me-order-book
        )