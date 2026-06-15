cmake_minimum_required(VERSION 3.22)

# ---- macOS deployment target ----
# Must be set BEFORE project() so the toolchain stamps the right minimum-OS
# into the Mach-O. Without this, building on a new macOS (e.g. Tahoe / macOS 26)
# marks the binary as requiring that same OS, so it crashes on load on every
# older Mac (newer-SDK symbols resolve to null -> EAccessViolation in the host).
# 10.13 is JUCE 8's minimum supported macOS. Applies to both universal slices.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
endif()

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
