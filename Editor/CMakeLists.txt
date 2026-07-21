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

# =============================================================================
# Editor UI vendors — the editor OWNS ImGui + tinyfiledialogs (D4). Built here from
# Editor/Vendors/*, so the engine never compiles or links them (runtime 0-imgui is structural).
# glfw stays ENGINE-owned (shared window infra, exported from OpaaxEngine.dll via _GLFW_BUILD_DLL);
# imgui imports it through GLFW_DLL + the engine's glfw headers, never a 2nd static copy (I1, L11).
# =============================================================================

# --- tinyfiledialogs ---------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/tinyfiledialogs/tinyfiledialogs.h")
    message(FATAL_ERROR
        "tinyfiledialogs not found in Editor/Vendors/tinyfiledialogs!\n"
        "Init the submodule: git submodule update --init Editor/Vendors/tinyfiledialogs"
    )
endif()
add_library(tinyfiledialogs STATIC Vendors/tinyfiledialogs/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs PUBLIC Vendors/tinyfiledialogs)
if(WIN32)
    target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32 user32 shell32)
endif()

# --- ImGui (OpenGL backend) --------------------------------------------------
# imgui_impl_vulkan is intentionally omitted: the live editor UI is OpenGL-only (the VK UI backend
# is in Legacy). Re-add it here, with Vulkan detection, when that path is revived.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui/imgui.h")
    message(FATAL_ERROR
        "ImGui not found in Editor/Vendors/imgui!\n"
        "Init the submodule: git submodule update --init Editor/Vendors/imgui"
    )
endif()
add_library(imgui STATIC
    Vendors/imgui/imgui.cpp
    Vendors/imgui/imgui_draw.cpp
    Vendors/imgui/imgui_tables.cpp
    Vendors/imgui/imgui_widgets.cpp
    Vendors/imgui/imgui_demo.cpp
    Vendors/imgui/backends/imgui_impl_glfw.cpp
    Vendors/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui
    PUBLIC
        Vendors/imgui
        Vendors/imgui/backends
        ${CMAKE_SOURCE_DIR}/Engine/Vendors/GLFW/include   # glfw stays engine-owned (shared)
)
target_compile_definitions(imgui
    PRIVATE
        IMGUI_IMPL_OPENGL_LOADER_GLAD
    PUBLIC
        GLFW_DLL   # import the single glfw instance exported by OpaaxEngine.dll (L11)
)
message(STATUS "[Opaax] Editor UI vendors: imgui + tinyfiledialogs built from Editor/Vendors/")

add_library(OpaaxEditorLib STATIC ${OPAAX_EDITOR_LIB_SOURCES})

# Engine (PUBLIC — editor headers expose engine types to editor exes) + the editor's own UI vendors.
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
