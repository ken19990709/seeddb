add_library(seeddb_buffer
    lru_replacer.cpp
    buffer_pool.cpp
)
target_include_directories(seeddb_buffer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(seeddb_buffer PUBLIC seeddb_common)
