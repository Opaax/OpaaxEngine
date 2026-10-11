# =============================================================================
# OpaaxEditorLib — the editor, as a static library. A game's editor executable is a thin
# subclass of EditorApplication linked against it (see opaax_add_game). The editor owns its UI
# vendors (ImGui, ImGuizmo, tinyfiledialogs): a runtime build never compiles or links them.
# Built only with OPAAX_EDITOR_SUPPORT.
# =============================================================================

file(GLOB_RECURSE OPAAX_EDITOR_LIB_SOURCES CONFIGURE_DEPENDS
    "Source/*.cpp"
    "Source/*.h"
    "Source/*.hpp"
)

function(opaax_require_editor_vendor InPath)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/${InPath}")
        message(FATAL_ERROR "Missing vendor file Editor/Vendors/${InPath}.\n"
                            "Run: git submodule update --init --recursive")
    endif()
endfunction()

# --- tinyfiledialogs ---------------------------------------------------------
opaax_require_editor_vendor("tinyfiledialogs/tinyfiledialogs.h")
add_library(tinyfiledialogs STATIC Vendors/tinyfiledialogs/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC Vendors/tinyfiledialogs)
if(WIN32)
    target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32 user32 shell32)
endif()

# --- ImGui (GLFW + OpenGL 3 backends) ----------------------------------------
opaax_require_editor_vendor("imgui/imgui.h")
add_library(imgui STATIC
    Vendors/imgui/imgui.cpp
    Vendors/imgui/imgui_draw.cpp
    Vendors/imgui/imgui_tables.cpp
    Vendors/imgui/imgui_widgets.cpp
    Vendors/imgui/imgui_demo.cpp
    Vendors/imgui/backends/imgui_impl_glfw.cpp
    Vendors/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC Vendors/imgui Vendors/imgui/backends)
# The engine owns the one GLFW instance; the ImGui backend uses it.
target_link_libraries(imgui PUBLIC glfw)

# --- ImGuizmo (transform gizmo) ----------------------------------------------
opaax_require_editor_vendor("ImGuizmo/src/ImGuizmo.h")
add_library(imguizmo STATIC Vendors/ImGuizmo/src/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC Vendors/ImGuizmo/src)
target_link_libraries(imguizmo PUBLIC imgui)
target_compile_definitions(imguizmo PUBLIC USE_IMGUI_API)

# --- The editor library ------------------------------------------------------
add_library(OpaaxEditorLib STATIC ${OPAAX_EDITOR_LIB_SOURCES})
opaax_configure_target(OpaaxEditorLib)

target_link_libraries(OpaaxEditorLib
    PUBLIC
        OpaaxEngine
        imgui
        imguizmo
        tinyfiledialogs
)

target_compile_definitions(OpaaxEditorLib PUBLIC OPAAX_WITH_EDITOR=1)
target_include_directories(OpaaxEditorLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
