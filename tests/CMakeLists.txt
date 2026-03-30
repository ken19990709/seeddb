# Test executable
add_executable(seeddb_tests)

# Link with Catch2 and libraries
target_link_libraries(seeddb_tests
    PRIVATE
        Catch2::Catch2WithMain
        seeddb_common
        seeddb_parser
        seeddb_storage
        seeddb_buffer
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
        unit/common/test_logical_type.cpp
        unit/common/test_value.cpp
        unit/parser/test_lexer.cpp
        unit/parser/test_ast.cpp
        unit/parser/test_parser.cpp
        unit/storage/test_storage.cpp
        unit/storage/test_page_id.cpp
        unit/storage/test_page_header.cpp
        unit/storage/test_page.cpp
        unit/storage/test_disk_manager.cpp
        unit/storage/test_page_manager.cpp
        unit/storage/test_storage_manager.cpp
        unit/storage/test_buffer_pool.cpp
        unit/storage/test_table_iterator.cpp
        unit/executor/test_executor_disk.cpp
        unit/executor/test_disk_stress.cpp
        ${CMAKE_SOURCE_DIR}/src/executor/executor.cpp
        ${CMAKE_SOURCE_DIR}/src/executor/function.cpp
)

target_include_directories(seeddb_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
