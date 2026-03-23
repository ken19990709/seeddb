add_library(seeddb_executor STATIC
    executor.cpp
    function.cpp
)
target_include_directories(seeddb_executor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(seeddb_executor PUBLIC seeddb_storage seeddb_parser seeddb_common)
