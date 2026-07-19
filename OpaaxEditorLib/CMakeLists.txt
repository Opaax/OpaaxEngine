# =============================================================================
# OpaaxEditorLib — the editor library (Editor.md D4/D8). Links Engine; owns ImGui.
# Built only under OPAAX_EDITOR_SUPPORT (added from the root gated on that option).
# STATIC: it archives editor code that editor executables link; ImGui is pulled into
# those exes, NEVER into the engine DLL or a runtime exe.
# =============================================================================

file(GLOB_RECURSE OPAAX_EDITOR_LIB_SOURCES CONFIGURE_DEPENDS
    "Source/*.cpp"
    "Source/*.h"
    "Source/*.hpp"
)

add_library(OpaaxEditorLib STATIC ${OPAAX_EDITOR_LIB_SOURCES})

# Engine (PUBLIC — editor headers expose engine types to editor exes).
# imgui + tinyfiledialogs (PUBLIC — the editor's UI backend is built on them; created in
# Engine/CMakeLists.txt under OPAAX_EDITOR_SUPPORT, so they exist by the time we link here).
target_link_libraries(OpaaxEditorLib
    PUBLIC
        OpaaxEngine
        imgui
        tinyfiledialogs
)

# The editor IS editor code — unlike the engine, it defines OPAAX_WITH_EDITOR=1.
target_compile_definitions(OpaaxEditorLib PUBLIC OPAAX_WITH_EDITOR=1)

target_include_directories(OpaaxEditorLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

message(STATUS "[Opaax] OpaaxEditorLib: static editor library target created")
