# SeedDB CLI executable

# CLI library (for testing)
add_library(seeddb_cli_lib STATIC
    formatter.cpp
    repl.cpp
)

target_include_directories(seeddb_cli_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(seeddb_cli_lib
    PUBLIC
        seeddb_common
        seeddb_parser
        seeddb_storage
        seeddb_executor
)

# CLI executable
add_executable(seeddb-cli
    main.cpp
)

target_link_libraries(seeddb-cli
    PRIVATE
        seeddb_cli_lib
)

set_target_properties(seeddb-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS seeddb-cli
    RUNTIME DESTINATION bin
)
