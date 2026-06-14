cmake_minimum_required(VERSION 3.22)

project(GrainFreeze VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Dependency management via CPM ----
include(cmake/cpm.cmake)

# Pull JUCE. Pin to a stable tag; bump as needed.
CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.12
)

add_subdirectory(plugin)

option(GRAINFREEZE_BUILD_TESTS "Build the GrainFreeze unit tests" OFF)

if(GRAINFREEZE_BUILD_TESTS)
    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        GIT_TAG v3.7.1
    )
    enable_testing()
    add_subdirectory(test)
endif()
