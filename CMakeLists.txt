cmake_minimum_required(VERSION 3.20)
project(OpaaxEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# One source encoding for the whole build (ARCHITECTURE.md I7). Without this MSVC decodes
# BOM'd files as UTF-8 and every other file as the ANSI code page — two encodings in one
# target. /utf-8 sets BOTH the source and the execution charset, so a literal means the same
# thing in every file and reaches the binary as UTF-8.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# A TU that registers every native resource type instantiates Load<T> for each of them (⑦-C
# P5b — the format entry bakes its typed acquire at Register<T>), and Engine.cpp crossed the
# 65k-section limit of the default object format (C1128). /bigobj lifts it; it changes nothing
# else about the build.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

# Options — editor and dev are TWO axes (ARCHITECTURE.md I12), one flag each.
#   OPAAX_EDITOR_SUPPORT : does OpaaxEditorLib + the editor exe exist?
#   OPAAX_DEV_BUILD      : where do assets resolve from — the source tree, or the exe dir?
option(OPAAX_EDITOR_SUPPORT "Build the editor library and editor executables" OFF)
option(OPAAX_DEV_BUILD      "Development build: resolve assets from the source tree, not the exe dir" OFF)
option(OPAAX_BUILD_TESTS    "Build the OpaaxTests unit-test target" ON)

# Editor implies dev; the reverse does not (that combination IS the `debug` preset). An editor built
# without the workspace bake would edit the deploy copy under bin/ instead of the source tree.
if(OPAAX_EDITOR_SUPPORT AND NOT OPAAX_DEV_BUILD)
    message(FATAL_ERROR "OPAAX_EDITOR_SUPPORT requires OPAAX_DEV_BUILD — the editor edits the SOURCE tree.")
endif()

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

# OpaaxEditorLib — the editor library (D4/D8). Built only when editor support is on; added
# AFTER Engine so its imgui/tinyfiledialogs vendor targets already exist to be linked.
if(OPAAX_EDITOR_SUPPORT)
    add_subdirectory(Editor)
endif()

#add_subdirectory(Game)
#add_subdirectory(SimpleShmup)
#add_subdirectory(PhysicsDemo)
add_subdirectory(Sandbox)
#add_subdirectory(MyProject)

# =============================================================================
# Visual Studio startup project — F5 launches the app the preset is FOR. One preset,
# one .sln (build/<preset>/), one answer:
#   debug-editor -> SandboxEditor.exe   debug / release -> Sandbox.exe
# Without this, VS defaults to ALL_BUILD / the first target. SandboxEditor exists only
# when editor support is on (gated in Sandbox/CMakeLists.txt), so the guard matches.
# =============================================================================
if(OPAAX_EDITOR_SUPPORT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SandboxEditor)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sandbox)
endif()
