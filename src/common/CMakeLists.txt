# Common utilities library (header-only for now)
add_library(seeddb_common INTERFACE)

target_include_directories(seeddb_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
