add_library(seeddb_executor INTERFACE)
target_include_directories(seeddb_executor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(seeddb_executor INTERFACE seeddb_storage seeddb_parser seeddb_common)
