cmake_minimum_required(VERSION 3.20)
project(OpaaxEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(OPAAX_EDITOR_SUPPORT "Include editor support in engine" OFF)
option(OPAAX_BUILD_EXAMPLES "Build example games" ON)
option(OPAAX_BUILD_TESTS    "Build the OpaaxTests unit-test target" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# CTest must be enabled at the top level so `ctest` finds tests registered deeper
# in the tree (Engine/Tests). Gated so a tests-off build stays clean.
if(OPAAX_BUILD_TESTS)
    enable_testing()
endif()

# Subdirectories
add_subdirectory(Engine)
add_subdirectory(Game)
add_subdirectory(SimpleShmup)
add_subdirectory(PhysicsDemo)
add_subdirectory(RendererStressDemo)