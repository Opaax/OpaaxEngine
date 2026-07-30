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
# Visual Studio startup project — make F5 launch the app the preset is FOR.
# Each preset generates its own .sln (build/<preset>/), so this is per-preset:
#   editor presets (OPAAX_EDITOR_SUPPORT=ON) -> SandboxEditor.exe (the editor)
#   runtime preset (OFF)                      -> Sandbox.exe       (the game)
# Without this, VS defaults to ALL_BUILD / the first target, so "Debug + Editor"
# would run the runtime Sandbox.exe despite its name — the mismatch this fixes.
# Set on the root directory (the .sln's top level). SandboxEditor exists only when
# editor support is on (it's gated in Sandbox/CMakeLists.txt), so the guard matches.
# =============================================================================
if(OPAAX_EDITOR_SUPPORT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SandboxEditor)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sandbox)
endif()
