# Test executable
add_executable(seeddb_tests)

# Link with Catch2 and common library
target_link_libraries(seeddb_tests
    PRIVATE
        Catch2::catch2_with_main
        seeddb_common
)

# Enable CTest integration with Catch2
include(Catch)
catch_discover_tests(seeddb_tests)

# Add test source files (will be populated later)
target_sources(seeddb_tests
    PRIVATE
        unit/common/test_placeholder.cpp
)
