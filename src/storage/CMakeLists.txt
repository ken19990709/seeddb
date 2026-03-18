add_library(seeddb_storage INTERFACE)
target_include_directories(seeddb_storage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(seeddb_storage INTERFACE seeddb_common)
