# Src Engine core
file(GLOB_RECURSE ENGINE_CORE_SOURCES
    "Source/Core/*.cpp"
    "Source/Core/*.h"
    "Source/Core/*.hpp"
    "Source/Systems/*.cpp"
    "Source/Systems/*.h"
    "Source/Systems/*.hpp"
    "Source/RHI/*.cpp"
    "Source/RHI/*.h"
    "Source/RHI/*.hpp"
    "Source/Platform/*.cpp"
    "Source/Platform/*.h"
    "Source/Platform/*.hpp"
    "Source/Renderer/*.cpp"
    "Source/Renderer/*.h"
    "Source/Renderer/*.hpp"
    "Source/Assets/*.cpp"
    "Source/Assets/*.h"
    "Source/Assets/*.hpp"
    "Source/Assets/*.cpp"
    "Source/Assets/*.hpp"
    "Source/ECS/*.cpp"
    "Source/ECS/*.h"
    "Source/ECS/*.hpp"
    "Source/Scene/*.cpp"
    "Source/Scene/*.h"
    "Source/Scene/*.hpp"
    "Source/World/*.cpp"
    "Source/World/*.h"
    "Source/World/*.hpp"
    "Source/Physics/*.cpp"
    "Source/Physics/*.h"
    "Source/Physics/*.hpp"
)

set(ENGINE_SOURCES ${ENGINE_CORE_SOURCES})

if(OPAAX_EDITOR_SUPPORT)
    file(GLOB_RECURSE ENGINE_EDITOR_SOURCES
        "Source/Editor/*.cpp"
        "Source/Editor/*.h"
    )
    list(APPEND ENGINE_SOURCES ${ENGINE_EDITOR_SOURCES})
    
    message(STATUS "Editor support enabled in engine")
endif()

# ==================================
# GLFW Configuration
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/GLFW/CMakeLists.txt")
    message(FATAL_ERROR 
        "GLFW not found in Engine/Vendors/GLFW!\n"
        "Please run: git clone https://github.com/glfw/glfw.git Engine/Vendors/GLFW"
    )
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# ADD GLFW subdirectory
add_subdirectory(Vendors/GLFW)

if(NOT TARGET glfw)
    message(FATAL_ERROR "GLFW target 'glfw' was not created!")
endif()

# ==================================
# END GLFW Configuration
# ==================================

# ==================================
# glad
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/glad/include/glad/glad.h")
    message(FATAL_ERROR
        "glad not found in Engine/Vendors/glad!\n"
        "Generate it at https://glad.dav1d.de (OpenGL 4.5 core, C/C++) and place\n"
        "include/ and src/ under Engine/Vendors/glad/"
    )
endif()
 
add_library(glad STATIC
    Vendors/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    Vendors/glad/include
)
# ==================================
# End glad
# ==================================

# ==================================
# tinyfiledialogs (editor only)
# ==================================
if(OPAAX_EDITOR_SUPPORT)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/tinyfiledialogs/tinyfiledialogs.h")
        message(FATAL_ERROR
            "tinyfiledialogs not found in Engine/Vendors/tinyfiledialogs!\n"
            "Drop tinyfiledialogs.h + tinyfiledialogs.c into that folder."
        )
    endif()

    add_library(tinyfiledialogs STATIC
        Vendors/tinyfiledialogs/tinyfiledialogs.c
    )

    target_include_directories(tinyfiledialogs PUBLIC
        Vendors/tinyfiledialogs
    )

    if(WIN32)
        target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32 user32 shell32)
    endif()

    message(STATUS "[Opaax] tinyfiledialogs: static lib created (editor mode)")
endif()
# ==================================
# END tinyfiledialogs
# ==================================

# ==================================
# spdlog Configuration (Header-Only)
# ==================================
# spdlog check
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/spdlog/include/spdlog/spdlog.h")
    message(FATAL_ERROR 
        "spdlog not found in Engine/Vendors/spdlog!\n"
        "Please run: git clone https://github.com/gabime/spdlog.git Engine/Vendors/spdlog"
    )
endif()

# spdlog header-only
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

# spdlog
add_subdirectory(Vendors/spdlog)

message(STATUS "spdlog: Header-Only mode")
# ==================================
# END spdlog Configuration (Header-Only)
# ==================================

# ==================================
# glm (Header-Only)
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/glm/glm/glm.hpp")
    message(FATAL_ERROR
        "glm not found in Engine/Vendors/glm!\n"
        "Please run: git clone https://github.com/g-truc/glm.git Engine/Vendors/glm"
    )
endif()
 
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE Vendors/glm)
message(STATUS "glm: header-only interface target created")
# ==================================
# END glm (Header-Only)
# ==================================


# ==================================
# stb_image / stb_truetype(Header-Only)
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/stb/stb_image.h")
    message(FATAL_ERROR
        "stb_image not found in Engine/Vendors/stb/stb_image.h!\n"
        "Download from: https://github.com/nothings/stb/blob/master/stb_image.h"
    )
endif()
 
# stb is header-only. We expose the include path as an interface library.
# STB_IMAGE_IMPLEMENTATION is defined in OpenGLTexture2D.cpp only — do NOT define
# it globally or you will get multiple definition linker errors.
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE Vendors)
message(STATUS "stb: header-only interface target created")
# ==================================
# END stb_image / stb_truetype(Header-Only)
# ==================================

# ==================================
# EnTT (Header-Only)
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/Entt/entt/single_include/entt/entt.hpp")
    message(FATAL_ERROR
        "EnTT not found in Engine/Vendors/Entt!\n"
        "Please run: git clone https://github.com/skypjack/entt.git Engine/Vendors/Entt"
    )
endif()

add_library(entt INTERFACE)
target_include_directories(entt INTERFACE Vendors/Entt/entt/single_include)
message(STATUS "EnTT: header-only interface target created")
# ==================================
# END EnTT
# ==================================

# ==================================
# nlohmann/json (Header-Only)
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/nlohmann/json.hpp")
    message(FATAL_ERROR
        "nlohmann/json not found in Engine/Vendors/nlohmann/json.hpp!\n"
        "Download from: https://github.com/nlohmann/json/releases"
    )
endif()

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE Vendors/nlohmann)
message(STATUS "nlohmann/json: header-only interface target created")
# ==================================
# END nlohmann/json
# ==================================

# ==================================
# Box2D (2D physics)
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/box2d/include/box2d/box2d.h")
    message(FATAL_ERROR
        "Box2D not found in Engine/Vendors/box2d!\n"
        "Please run: git clone https://github.com/erincatto/box2d.git Engine/Vendors/box2d"
    )
endif()

# Box2D guards its samples/tests/benchmarks behind PROJECT_IS_TOP_LEVEL, so as a
# sub-directory only the 'box2d' static lib target is built — nothing to turn off.
add_subdirectory(Vendors/box2d)
message(STATUS "Box2D: static lib target 'box2d' created")
# ==================================
# END Box2D
# ==================================

# ==================================
# ImGui
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui/imgui.h")
    message(FATAL_ERROR
        "ImGui not found in Engine/Vendors/imgui!\n"
        "Please run: git clone https://github.com/ocornut/imgui.git Engine/Vendors/imgui"
    )
endif()

if(OPAAX_EDITOR_SUPPORT)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui/imgui.h")
        message(FATAL_ERROR
            "ImGui not found in Engine/Vendors/imgui!\n"
            "Please run: git clone https://github.com/ocornut/imgui.git Engine/Vendors/imgui"
        )
    endif()

    # Base ImGui + platform/OpenGL renderer backends. imgui_impl_vulkan.cpp is attached later,
    # gated on the Vulkan SDK being present (see the Vulkan detection block below).
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
            Vendors/GLFW/include
    )

    target_compile_definitions(imgui
        PRIVATE
            IMGUI_IMPL_OPENGL_LOADER_GLAD
    )

    message(STATUS "[Opaax] ImGui: static lib created (editor mode)")
else()
    message(STATUS "[Opaax] ImGui: skipped (release mode)")
endif()
# ==================================
# END ImGui
# ==================================

# ==================================
# Dll Engine
# ==================================
add_library(OpaaxEngine SHARED ${ENGINE_SOURCES})

# ENGINE_EXPORTS
target_compile_definitions(OpaaxEngine PRIVATE ENGINE_EXPORTS)

# Editor
if(OPAAX_EDITOR_SUPPORT)
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WITH_EDITOR=1)
    # Workspace root — the source tree holding Engine/ + every game project. OpaaxPath derives
    # the engine root (<workspace>/Engine) and the per-app project root (<workspace>/<AppName>)
    # from it. Editor-only on purpose: release binaries must not bake in absolute build-host
    # paths (they resolve to the exe dir, whose deploy mirrors the workspace layout).
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WORKSPACE_DIR="${CMAKE_SOURCE_DIR}")
    message(STATUS "[Opaax] Editor support: ON")
    message(STATUS "[Opaax] Workspace root: ${CMAKE_SOURCE_DIR}")
else()
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WITH_EDITOR=0)
    message(STATUS "[Opaax] Editor support: OFF (Release build)")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_DEBUG)
    message(STATUS "[Opaax] Debug mode: ON")
endif()

# ==================================
# glslang (GLSL -> SPIR-V) — OPTIONAL, from the Vulkan SDK.
#   Present : shaders compile to SPIR-V at load (RHI/ShaderCompiler); OpenGL consumes it
#             via GL_ARB_gl_spirv, and the SPIR-V is Vulkan-ready (-> OPAAX_HAS_GLSLANG=1).
#   Absent  : the engine still builds + runs on OpenGL using the GLSL fallback in
#             OpenGLShader; a Vulkan backend simply cannot be used (-> OPAAX_HAS_GLSLANG=0).
# Detection is robust (no hardcoded path): find_package(Vulkan) locates the SDK via its
# standard mechanisms; the SDK root + Lib dir are derived from the found include dir.
# NOTE: the SDK's glslang-config.cmake chains to a SPIRV-Tools package config the SDK does
# not ship, so we link the static libs by path (Debug -> d-suffixed /MDd libs).
# ==================================
set(OPAAX_HAS_GLSLANG OFF)
set(OPAAX_GLSLANG_INCLUDE "")
set(OPAAX_GLSLANG_LINK "")

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    # SDK root = parent of the Vulkan include dir (e.g. <root>/Include -> <root>).
    get_filename_component(OPAAX_VULKAN_ROOT "${Vulkan_INCLUDE_DIRS}" DIRECTORY)
    set(_glslang_inc "${OPAAX_VULKAN_ROOT}/Include")
    set(_glslang_lib "${OPAAX_VULKAN_ROOT}/Lib")

    # Only enable if the glslang static libs + headers actually shipped with this SDK.
    if(EXISTS "${_glslang_lib}/glslang.lib" AND EXISTS "${_glslang_inc}/glslang/Public/ShaderLang.h")
        set(OPAAX_HAS_GLSLANG ON)
        set(OPAAX_GLSLANG_INCLUDE "${_glslang_inc}")

        # Dependents before dependencies. glslang-default-resource-limits -> glslang ->
        # {MachineIndependent, SPIRV, GenericCodeGen, OSDependent}; SPIRV -> SPIRV-Tools-opt
        # -> SPIRV-Tools.
        set(OPAAX_GLSLANG_LIB_NAMES
            glslang-default-resource-limits
            glslang
            MachineIndependent
            SPIRV
            SPIRV-Tools-opt
            SPIRV-Tools
            GenericCodeGen
            OSDependent
        )
        foreach(_l ${OPAAX_GLSLANG_LIB_NAMES})
            list(APPEND OPAAX_GLSLANG_LINK
                "$<$<CONFIG:Debug>:${_glslang_lib}/${_l}d.lib>"
                "$<$<NOT:$<CONFIG:Debug>>:${_glslang_lib}/${_l}.lib>")
        endforeach()
    endif()
endif()

if(OPAAX_HAS_GLSLANG)
    message(STATUS "[Opaax] glslang found — SPIR-V shader path ENABLED (Vulkan-ready).")
else()
    message(STATUS "[Opaax] glslang/Vulkan SDK not found — OpenGL-only build using the GLSL "
                   "shader fallback. The Vulkan backend will be unavailable.")
endif()

# ==================================
# Vulkan backend — OPTIONAL, from the Vulkan SDK.
#   Present : the Vulkan RHI backend (Source/RHI/Vulkan/) compiles and EBackend::Vulkan is
#             selectable. vk-bootstrap (vendored) handles instance/device/swapchain; VMA (shipped
#             in the SDK include dir) handles device memory; vulkan-1 is the loader. -> OPAAX_HAS_VULKAN=1
#   Absent  : every Source/RHI/Vulkan TU compiles to nothing (#if OPAAX_HAS_VULKAN), the engine is
#             OpenGL-only, and selecting "Vulkan" in config falls back to OpenGL. -> OPAAX_HAS_VULKAN=0
# Reuses find_package(Vulkan) + OPAAX_VULKAN_ROOT from the glslang block above (no duplicate find).
# ==================================
set(OPAAX_HAS_VULKAN OFF)
set(OPAAX_VULKAN_INCLUDE "")
set(OPAAX_VULKAN_LINK "")

if(Vulkan_FOUND AND EXISTS "${OPAAX_VULKAN_ROOT}/Include/vulkan/vulkan.h"
                AND EXISTS "${OPAAX_VULKAN_ROOT}/Include/vma/vk_mem_alloc.h")
    set(OPAAX_HAS_VULKAN ON)
    set(OPAAX_VULKAN_INCLUDE "${OPAAX_VULKAN_ROOT}/Include")   # vulkan/ and vma/ both live here

    # vk-bootstrap (vendored source) as a static lib. Needs the SDK Vulkan headers; loads the
    # loader (vulkan-1) dynamically via vkGetInstanceProcAddr.
    add_library(vk-bootstrap STATIC
        Vendors/vk-bootstrap/src/VkBootstrap.cpp
    )
    target_include_directories(vk-bootstrap PUBLIC
        Vendors/vk-bootstrap/src
        "${OPAAX_VULKAN_INCLUDE}"
    )
    target_link_libraries(vk-bootstrap PUBLIC ${Vulkan_LIBRARIES})

    set(OPAAX_VULKAN_LINK vk-bootstrap ${Vulkan_LIBRARIES})
    message(STATUS "[Opaax] Vulkan SDK found — Vulkan backend ENABLED (vk-bootstrap + VMA).")

    # Editor: compile the ImGui Vulkan renderer backend (consumed by VulkanEditorUIBackend). The
    # imgui static lib is created earlier, only under OPAAX_EDITOR_SUPPORT — attach the TU here so
    # it stays gated on the SDK being present (mirrors the OpenGL-only default).
    if(OPAAX_EDITOR_SUPPORT)
        target_sources(imgui PRIVATE Vendors/imgui/backends/imgui_impl_vulkan.cpp)
        target_include_directories(imgui PRIVATE "${OPAAX_VULKAN_INCLUDE}")
        message(STATUS "[Opaax] ImGui: Vulkan renderer backend ENABLED (imgui_impl_vulkan).")
    endif()
else()
    message(STATUS "[Opaax] Vulkan SDK not found — Vulkan backend DISABLED (OpenGL-only).")
endif()

# ==================================
# Link
# ==================================

target_link_libraries(OpaaxEngine
    PUBLIC
        glfw
        spdlog::spdlog_header_only
        glm
        stb
        glad
        entt
        nlohmann_json
        box2d
)

target_link_libraries(OpaaxEngine
    PRIVATE
        ${OPAAX_GLSLANG_LINK}
        ${OPAAX_VULKAN_LINK}
)

# 1 when glslang is linked (SPIR-V shader path), 0 otherwise (GLSL fallback).
target_compile_definitions(OpaaxEngine PRIVATE OPAAX_HAS_GLSLANG=$<BOOL:${OPAAX_HAS_GLSLANG}>)

# 1 when the Vulkan SDK is present (Vulkan backend compiled), 0 otherwise (OpenGL-only).
target_compile_definitions(OpaaxEngine PRIVATE OPAAX_HAS_VULKAN=$<BOOL:${OPAAX_HAS_VULKAN}>)

if(OPAAX_EDITOR_SUPPORT)
    target_link_libraries(OpaaxEngine PRIVATE imgui tinyfiledialogs)
endif()

# Include directories
target_include_directories(OpaaxEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Private
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/GLFW/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/spdlog/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/Entt/entt/single_include
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/box2d/include
        ${OPAAX_GLSLANG_INCLUDE}
        ${OPAAX_VULKAN_INCLUDE}                                  # vulkan/ + vma/ (empty if no SDK)
        ${CMAKE_CURRENT_SOURCE_DIR}/Vendors/vk-bootstrap/src     # VkBootstrap.h (guarded by OPAAX_HAS_VULKAN)
)

if(WIN32)
    target_compile_definitions(OpaaxEngine PRIVATE _WIN32)
    
    target_link_libraries(OpaaxEngine PRIVATE
        glad
        opengl32
    )
endif()

# ==================================
# Copy engine assets post-build
# ==================================
set(ENGINE_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(ENGINE_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/Engine/Assets)


add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ENGINE_ASSETS_DEST}
    COMMENT "Creating Engine/Assets directory..."
)

set(ENGINE_MANIFEST_SOURCE ${ENGINE_ASSETS_SOURCE}/AssetManifest.json)

if(NOT EXISTS ${ENGINE_MANIFEST_SOURCE})
    message(STATUS "[Opaax] Engine manifest not found — generating empty manifest.")
    file(WRITE ${ENGINE_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${ENGINE_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ENGINE_ASSETS_SOURCE}/AssetManifest.json
        ${ENGINE_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying engine asset manifest..."
)

# Asset dirs are copied only when they exist on disk. An empty asset folder is not
# tracked by git, so a fresh clone (e.g. CI) legitimately lacks it — an unconditional
# copy_directory would then fail with "cannot find the path specified".
# Shader
if(EXISTS ${ENGINE_ASSETS_SOURCE}/Shaders)
    add_custom_command(TARGET OpaaxEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ENGINE_ASSETS_SOURCE}/Shaders
            ${ENGINE_ASSETS_DEST}/Shaders
        COMMENT "Copying engine shaders..."
    )
endif()

# Texture
if(EXISTS ${ENGINE_ASSETS_SOURCE}/Textures)
    add_custom_command(TARGET OpaaxEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ENGINE_ASSETS_SOURCE}/Textures
            ${ENGINE_ASSETS_DEST}/Textures
        COMMENT "Copying engine textures..."
    )
endif()

# Mesh
if(EXISTS ${ENGINE_ASSETS_SOURCE}/Meshes)
    add_custom_command(TARGET OpaaxEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ENGINE_ASSETS_SOURCE}/Meshes
            ${ENGINE_ASSETS_DEST}/Meshes
        COMMENT "Copying engine meshes..."
    )
endif()

# ==================================
# Unit tests (OpaaxTests) — links the engine DLL like any other consumer.
# ==================================
if(OPAAX_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

# Font
if(EXISTS ${ENGINE_ASSETS_SOURCE}/Fonts)
    add_custom_command(TARGET OpaaxEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ENGINE_ASSETS_SOURCE}/Fonts
            ${ENGINE_ASSETS_DEST}/Fonts
        COMMENT "Copying engine fonts..."
    )
endif()

# ==================================
# Shader compile
# ==================================
#if(WIN32)
#    # Trouver le compilateur de shaders DirectX
#    find_program(DXC_EXECUTABLE dxc.exe
#        HINTS
#            "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
#            "$ENV{WindowsSdkDir}/bin/*/x64"
#    )
#
#    if(DXC_EXECUTABLE)
#        # Liste des shaders à compiler
#        set(SHADER_FILES
#            ${ENGINE_ASSETS_SOURCE}/Shaders/Sprite.hlsl
#            ${ENGINE_ASSETS_SOURCE}/Shaders/UI.hlsl
#            ${ENGINE_ASSETS_SOURCE}/Shaders/Debug.hlsl
#        )
#
#        foreach(SHADER ${SHADER_FILES})
#            get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
#            
#            # Compiler vertex shader
#            add_custom_command(TARGET OpaaxEngine POST_BUILD
#                COMMAND ${DXC_EXECUTABLE} -T vs_6_0 -E VSMain
#                    ${SHADER}
#                    -Fo ${ENGINE_ASSETS_DEST}/Shaders/${SHADER_NAME}_VS.cso
#                COMMENT "Compiling ${SHADER_NAME} vertex shader..."
#            )
#            
#            # Compiler pixel shader
#            add_custom_command(TARGET OpaaxEngine POST_BUILD
#                COMMAND ${DXC_EXECUTABLE} -T ps_6_0 -E PSMain
#                    ${SHADER}
#                    -Fo ${ENGINE_ASSETS_DEST}/Shaders/${SHADER_NAME}_PS.cso
#                COMMENT "Compiling ${SHADER_NAME} pixel shader..."
#            )
#        endforeach()
#    endif()
#endif()