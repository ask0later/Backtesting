add_library(me-csv-table CsvTable.cpp)

target_include_directories(me-csv-table PUBLIC
    ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(me-csv-table PUBLIC rapidcsv::rapidcsv)