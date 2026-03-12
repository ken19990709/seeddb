# SQL Parser library
add_library(seeddb_parser STATIC
    lexer.cpp
)

target_include_directories(seeddb_parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(seeddb_parser
    PUBLIC
        seeddb_common
)
