# Src Engine core
file(GLOB_RECURSE ENGINE_CORE_SOURCES CONFIGURE_DEPENDS
    # Gregory runtime layers (Phase B). Core/ = foundations (Config, Log, Container, Jobs, Time,
    # Serialize, Events, Systems/Subsystem.h + Core/*.h). Application/ Engine/ World/ were promoted
    # out of the old Core/ junk-drawer into top-level layer dirs.
    "Source/Core/*.cpp"
    "Source/Core/*.h"
    "Source/Core/*.hpp"
    "Source/Core/*.inl"
    "Source/Application/*.cpp"
    "Source/Application/*.h"
    "Source/Application/*.hpp"
    "Source/Engine/*.cpp"
    "Source/Engine/*.h"
    "Source/Engine/*.hpp"
    "Source/World/*.cpp"
    "Source/World/*.h"
    "Source/World/*.hpp"
    "Source/RHI/*.cpp"
    "Source/RHI/*.h"
    "Source/RHI/*.hpp"
    "Source/Physics/*.cpp"
    "Source/Physics/*.h"
    "Source/Physics/*.hpp"
    "Source/Platform/*.cpp"
    "Source/Platform/*.h"
    "Source/Platform/*.hpp"
    "Source/Renderer/*.cpp"
    "Source/Renderer/*.h"
    "Source/Renderer/*.hpp"
)

set(ENGINE_SOURCES ${ENGINE_CORE_SOURCES})

# D4: the engine DLL has ZERO editor knowledge. Source/Editor/** is NEVER globbed here —
# it belongs to OpaaxEditorLib. (This empty marker exists so the old glob is not reintroduced.)

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

# GLFW has ONE global state (glfwInit, the window list, the current context). OpaaxEngine is a DLL
# and OWNS that state — it calls glfwInit and creates the window. The editor's ImGui GLFW backend
# lives in the exe and MUST see the SAME glfw, or it hits an uninitialised second copy (glfwGetWin32Window
# returns null -> ImGui_ImplGlfw assert). So the engine DLL EXPORTS glfw as part of its ABI, using glfw's
# OWN dllexport switch (no glfw sources changed): _GLFW_BUILD_DLL makes GLFWAPI __declspec(dllexport), so
# the glfw symbols compiled into OpaaxEngine.dll are exported. Consumers set GLFW_DLL (dllimport) instead of
# relinking a second static glfw (see the imgui target + the PRIVATE link below). One instance, no glfw.dll
# shipped, runtime deployment unchanged. Same "one instance across the DLL line" rule as ARCHITECTURE.md I1/I2.
target_compile_definitions(glfw PUBLIC _GLFW_BUILD_DLL)

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
# ImGui + tinyfiledialogs — EDITOR-OWNED (D4)
# ==================================
# The engine build no longer knows about ImGui or tinyfiledialogs. Both are editor UI vendors,
# built in Editor/CMakeLists.txt from Editor/Vendors/* and linked into OpaaxEditorLib. This makes
# "runtime carries zero imgui" STRUCTURAL (the engine can't link what it never builds), not just a
# dumpbin convention. glfw stays engine-owned (shared window infra, exported from OpaaxEngine.dll —
# see _GLFW_BUILD_DLL above); the editor's imgui imports it via GLFW_DLL.
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
# STB_IMAGE_IMPLEMENTATION is defined in TextureResource.cpp only — do NOT define
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

# NOTE: ImGui target moved to Editor/CMakeLists.txt (editor-owned — see the D4 note above).

# ==================================
# Dll Engine
# ==================================
add_library(OpaaxEngine SHARED ${ENGINE_SOURCES})

# ENGINE_EXPORTS
target_compile_definitions(OpaaxEngine PRIVATE ENGINE_EXPORTS)

# D4: the engine never knows about the editor. OPAAX_WITH_EDITOR is ALWAYS 0 on the engine DLL —
# the editor lives entirely in OpaaxEditorLib. (EngineAPI.h defaults it to 0; we assert it here.)
target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WITH_EDITOR=0)

# Workspace root — a dev build bakes the source-tree root so Paths can resolve source assets/configs;
# a ship build must NOT bake a build-host path (it resolves from the exe dir, whose deploy mirrors the
# workspace layout). Keyed on OPAAX_DEV_BUILD, never on the editor (I12): a debug GAME build is a dev
# build with no editor. IPaths keys on defined(OPAAX_WORKSPACE_DIR), not on OPAAX_WITH_EDITOR.
if(OPAAX_DEV_BUILD)
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WORKSPACE_DIR="${CMAKE_SOURCE_DIR}")
    message(STATUS "[Opaax] Workspace root baked (dev build): ${CMAKE_SOURCE_DIR}")
else()
    message(STATUS "[Opaax] Ship build: no workspace path baked (exe-dir resolution)")
endif()

# Per-CONFIG, not CMAKE_BUILD_TYPE: the VS generator is multi-config, where CMAKE_BUILD_TYPE means
# nothing at build time and the compiled config comes from --config.
target_compile_definitions(OpaaxEngine PUBLIC $<$<CONFIG:Debug>:OPAAX_DEBUG>)

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

    # NOTE: the ImGui Vulkan renderer backend (imgui_impl_vulkan) is NOT built — imgui is editor-owned
    # now (Editor/CMakeLists.txt) and the live editor UI is OpenGL-only (the VK UI backend is in Legacy).
    # Re-add it editor-side, with Vulkan detection, when the VK editor UI is revived.
else()
    message(STATUS "[Opaax] Vulkan SDK not found — Vulkan backend DISABLED (OpenGL-only).")
endif()

# ==================================
# Link
# ==================================

target_link_libraries(OpaaxEngine
    PUBLIC
        spdlog::spdlog_header_only
        glm
        stb
        glad
        entt
        nlohmann_json
)

# glfw is PRIVATE (not PUBLIC): it is embedded in + EXPORTED from OpaaxEngine.dll (_GLFW_BUILD_DLL above).
# PUBLIC would make every consumer statically relink a SECOND glfw copy with its own uninitialised global
# state — the exact bug this fixes. No public engine header includes <GLFW/glfw3.h> (only .cpp + the private
# WindowsWindow.h), so nothing downstream needs glfw's headers transitively.
#
# box2d is PRIVATE for the SAME reason (L11), moved off PUBLIC when the physics seam landed: it is a static
# lib with GLOBAL state (b2SetLengthUnitsPerMeter), so PUBLIC gave every host its own copy of it. Latent
# only because nothing outside the engine included box2d — the seam is what makes that permanent. Physics is
# reached through the OPAAX_API IPhysicsWorld / PhysicsAPI::Create, so the one instance lives in the DLL and
# no consumer needs box2d's headers or its symbols (its include dir was already PRIVATE).
target_link_libraries(OpaaxEngine
    PRIVATE
        glfw
        box2d
        ${OPAAX_GLSLANG_LINK}
        ${OPAAX_VULKAN_LINK}
)

# 1 when glslang is linked (SPIR-V shader path), 0 otherwise (GLSL fallback).
target_compile_definitions(OpaaxEngine PRIVATE OPAAX_HAS_GLSLANG=$<BOOL:${OPAAX_HAS_GLSLANG}>)

# 1 when the Vulkan SDK is present (Vulkan backend compiled), 0 otherwise (OpenGL-only).
target_compile_definitions(OpaaxEngine PRIVATE OPAAX_HAS_VULKAN=$<BOOL:${OPAAX_HAS_VULKAN}>)

# D4: imgui + tinyfiledialogs are neither built nor linked by the engine — they are editor-owned
# (Editor/CMakeLists.txt, from Editor/Vendors/*). The engine can't leak a symbol it never compiles;
# the `dumpbin` gate on Sandbox.exe still guards it.

# Include directories
target_include_directories(OpaaxEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Private
        # D4: no imgui include paths on the engine — ImGui belongs to OpaaxEditorLib (S1).
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

# What the runtime reads out of Engine/Assets: Shaders (Renderer2D::Init), Textures, SpriteSheets and
# Fonts — the prototyping content a game references through the "/Engine/" mount (IPaths::ENGINE_MOUNT),
# which a release build resolves from the EXE dir rather than the source tree (I12). Meshes /
# AssetManifest.json belonged to the retired Legacy/Assets manifest world and went in the
# 2026-08-21 cleanup along with their copy steps; Fonts came BACK with ⑥ S4's text rendering, which
# reads them as real resources rather than through a manifest.
# ADDING A MOUNTED FOLDER MEANS ADDING IT HERE. Content that resolves in a dev build and to nothing
# in a shipped one is the failure this list exists to prevent, and it is silent.
# Guarded on EXISTS because an empty asset folder is not tracked by git, so a fresh clone
# (e.g. CI) legitimately lacks it and an unconditional copy_directory would fail.
foreach(ENGINE_ASSET_DIR Shaders Textures SpriteSheets Fonts)
    if(EXISTS ${ENGINE_ASSETS_SOURCE}/${ENGINE_ASSET_DIR})
        add_custom_command(TARGET OpaaxEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${ENGINE_ASSETS_SOURCE}/${ENGINE_ASSET_DIR}
                ${ENGINE_ASSETS_DEST}/${ENGINE_ASSET_DIR}
            COMMENT "Copying engine ${ENGINE_ASSET_DIR}..."
        )
    endif()
endforeach()

# ==================================
# Unit tests (OpaaxTests) — links the engine DLL like any other consumer.
# ==================================
if(OPAAX_BUILD_TESTS)
    add_subdirectory(Tests)
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