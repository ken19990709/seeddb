cmake_minimum_required(VERSION 3.16)

project(SeedDB
    VERSION 0.1.0
    DESCRIPTION "A lightweight educational database kernel"
    LANGUAGES CXX
)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type (default to Debug)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Detect and enable CCache for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Compiler warnings (GCC/Clang)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing
enable_testing()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Fetch Catch2 testing framework (must be done before adding tests subdirectory)
include(Catch2)

# Add subdirectories (src first, then tests which depends on Catch2)
add_subdirectory(src)
add_subdirectory(tests)

# Compilation optimizations for seeddb_common
target_precompile_headers(seeddb_common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h)
set_target_properties(seeddb_common PROPERTIES UNITY_BUILD ON)

# Compilation optimizations for seeddb_parser
target_precompile_headers(seeddb_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h)
set_target_properties(seeddb_parser PROPERTIES UNITY_BUILD ON)

# Print configuration summary
message(STATUS "")
message(STATUS "SeedDB Configuration Summary")
message(STATUS "  Version:    ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
