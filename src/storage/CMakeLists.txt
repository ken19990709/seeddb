# Build buffer pool first (storage_manager depends on it)
add_subdirectory(buffer)

add_library(seeddb_storage
    disk_manager.cpp
    page_manager.cpp
    storage_manager.cpp
    table_iterator.cpp
)
target_include_directories(seeddb_storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(seeddb_storage PUBLIC seeddb_common seeddb_buffer)
