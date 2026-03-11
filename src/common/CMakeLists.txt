# Common utilities library
add_library(seeddb_common STATIC
    error.cpp
)

target_include_directories(seeddb_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
