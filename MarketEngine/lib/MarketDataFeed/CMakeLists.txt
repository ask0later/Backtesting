add_library(me-data-feed EventSource.cpp)

target_include_directories(me-data-feed PUBLIC
    ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(me-data-feed PUBLIC me-csv-table)