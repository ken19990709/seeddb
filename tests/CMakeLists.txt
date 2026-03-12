# Test executable
add_executable(seeddb_tests)

# Link with Catch2 and libraries
target_link_libraries(seeddb_tests
    PRIVATE
        Catch2::Catch2WithMain
        seeddb_common
        seeddb_parser
)

# Enable CTest integration with Catch2
include(Catch)
catch_discover_tests(seeddb_tests)

# Add test source files
target_sources(seeddb_tests
    PRIVATE
        unit/common/test_placeholder.cpp
        unit/common/test_types.cpp
        unit/common/test_error.cpp
        unit/common/test_logger.cpp
        unit/common/test_config.cpp
        unit/parser/test_lexer.cpp
)
