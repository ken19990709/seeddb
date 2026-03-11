# Fetch Catch2 v3.x from GitHub
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(Catch2)

# Include Catch2's CMake module for test discovery
# Note: The path uses lowercase 'catch2' for the populated directory
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Debug: Show available targets
message(STATUS "Catch2 source dir: ${catch2_SOURCE_DIR}")
