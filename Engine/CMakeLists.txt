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
# stb_image (Header-Only)
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
# END stb_image (Header-Only)
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
# ImGui
# ==================================
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/imgui/imgui.h")
    message(FATAL_ERROR
        "ImGui not found in Engine/Vendors/imgui!\n"
        "Please run: git clone https://github.com/ocornut/imgui.git Engine/Vendors/imgui"
    )
endif()

add_library(imgui STATIC
    Vendors/imgui/imgui.cpp
    Vendors/imgui/imgui_draw.cpp
    Vendors/imgui/imgui_tables.cpp
    Vendors/imgui/imgui_widgets.cpp
    Vendors/imgui/imgui_demo.cpp
    # GLFW + OpenGL3 backends
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

message(STATUS "ImGui: static lib created")
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
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_DEBUG)
else()
    target_compile_definitions(OpaaxEngine PUBLIC OPAAX_WITH_EDITOR=0)
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
        imgui
)

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
set(ENGINE_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/EngineAssets)


add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ENGINE_ASSETS_DEST}
    COMMENT "Creating EngineAssets directory..."
)

# Shader
add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ENGINE_ASSETS_SOURCE}/Shaders
        ${ENGINE_ASSETS_DEST}/Shaders
    COMMENT "Copying engine shaders..."
)

# Texture
add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ENGINE_ASSETS_SOURCE}/Textures
        ${ENGINE_ASSETS_DEST}/Textures
    COMMENT "Copying engine textures..."
)

# Mesh
add_custom_command(TARGET OpaaxEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ENGINE_ASSETS_SOURCE}/Meshes
        ${ENGINE_ASSETS_DEST}/Meshes
    COMMENT "Copying engine meshes..."
)

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