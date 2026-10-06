cmake_minimum_required(VERSION 3.24)
project(OpaaxEngine VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# =============================================================================
# Options
#   OPAAX_EDITOR_SUPPORT : build the editor library and the <Game>Editor executables.
#   OPAAX_DEV_BUILD      : assets resolve from the source tree instead of next to the executable.
#   OPAAX_BUILD_TESTS    : build the unit tests and register them with CTest.
# =============================================================================
option(OPAAX_EDITOR_SUPPORT "Build the editor library and editor executables" OFF)
option(OPAAX_DEV_BUILD      "Development build: resolve assets from the source tree, not the exe dir" OFF)
option(OPAAX_BUILD_TESTS    "Build the unit tests" ON)

# The editor edits the source tree, so it only makes sense in a dev build.
if(OPAAX_EDITOR_SUPPORT AND NOT OPAAX_DEV_BUILD)
    message(FATAL_ERROR "OPAAX_EDITOR_SUPPORT requires OPAAX_DEV_BUILD: the editor edits the source tree.")
endif()

include(OpaaxCompilerSettings)
include(OpaaxGame)

# Output directories. Multi-config generators add a per-config sub-folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(OPAAX_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(Engine)

if(OPAAX_EDITOR_SUPPORT)
    add_subdirectory(Editor)
endif()

# Game projects. Each one is a folder with a CMakeLists.txt calling opaax_add_game().
add_subdirectory(Sandbox)

# Visual Studio: F5 starts the app the preset is for.
if(OPAAX_EDITOR_SUPPORT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SandboxEditor)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sandbox)
endif()
