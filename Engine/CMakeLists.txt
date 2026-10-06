# =============================================================================
# OpaaxEngine — the engine, as a static library (OpaaxEngineLib) linked whole through the
# OpaaxEngine target. Game executables, the editor and the tests link OpaaxEngine.
# Every third-party library it uses lives in Engine/Vendors.
# =============================================================================

file(GLOB_RECURSE OPAAX_ENGINE_SOURCES CONFIGURE_DEPENDS
    "Source/*.cpp"
    "Source/*.h"
    "Source/*.hpp"
    "Source/*.inl"
)

# Fails early with a readable message when a vendor folder is missing (usually: submodules not
# initialized).
function(opaax_require_vendor InPath InHint)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/${InPath}")
        message(FATAL_ERROR "Missing vendor file Engine/Vendors/${InPath}.\n${InHint}")
    endif()
endfunction()

set(OPAAX_SUBMODULE_HINT "Run: git submodule update --init --recursive")

# ==================================
# GLFW — window, input and GL context
# ==================================
opaax_require_vendor("GLFW/CMakeLists.txt" "GLFW is vendored as a folder; restore Engine/Vendors/GLFW.")
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
# Laptops with two GPUs run the game on the fast one.
set(GLFW_USE_HYBRID_HPG ON  CACHE BOOL "" FORCE)
add_subdirectory(Vendors/GLFW)

# ==================================
# glad — OpenGL function loader
# ==================================
opaax_require_vendor("glad/include/glad/glad.h" "Restore Engine/Vendors/glad (OpenGL core loader).")
add_library(glad STATIC Vendors/glad/src/glad.c)
target_include_directories(glad PUBLIC Vendors/glad/include)

# ==================================
# Header-only libraries
# ==================================
opaax_require_vendor("spdlog/include/spdlog/spdlog.h" "${OPAAX_SUBMODULE_HINT}")
set(SPDLOG_FMT_EXTERNAL   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendors/spdlog)

opaax_require_vendor("glm/glm/glm.hpp" "${OPAAX_SUBMODULE_HINT}")
add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE Vendors/glm)

# stb_image and stb_truetype. Each implementation is compiled in exactly one engine .cpp.
opaax_require_vendor("stb/stb_image.h" "Restore Engine/Vendors/stb.")
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE Vendors)

opaax_require_vendor("Entt/entt/single_include/entt/entt.hpp" "${OPAAX_SUBMODULE_HINT}")
add_library(entt INTERFACE)
target_include_directories(entt SYSTEM INTERFACE Vendors/Entt/entt/single_include)

opaax_require_vendor("nlohmann/include/nlohmann/json.hpp" "Restore Engine/Vendors/nlohmann.")
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE Vendors/nlohmann/include)

# miniaudio. The implementation is compiled in exactly one engine .cpp.
opaax_require_vendor("miniaudio/miniaudio.h" "Restore Engine/Vendors/miniaudio.")
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio SYSTEM INTERFACE Vendors/miniaudio)

# ==================================
# Box2D — 2D physics
# ==================================
opaax_require_vendor("box2d/include/box2d/box2d.h" "${OPAAX_SUBMODULE_HINT}")
set(BOX2D_SAMPLES    OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS       OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendors/box2d)

# ==================================
# OpenGL
# ==================================
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# ==================================
# The engine library
# ==================================
add_library(OpaaxEngineLib STATIC ${OPAAX_ENGINE_SOURCES})
opaax_configure_target(OpaaxEngineLib)

target_include_directories(OpaaxEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# Engine headers expose glm, entt, nlohmann and spdlog types, so those are PUBLIC.
# The others are implementation details: no engine header includes them.
target_link_libraries(OpaaxEngineLib
    PUBLIC
        glm
        entt
        nlohmann_json
        spdlog::spdlog_header_only
        Threads::Threads
    PRIVATE
        glfw
        glad
        stb
        box2d
        miniaudio
        OpenGL::GL
        ${CMAKE_DL_LIBS}
)

if(WIN32)
    # Crash handler: minidumps and symbolized stacks.
    target_link_libraries(OpaaxEngineLib PRIVATE dbghelp)
endif()

if(APPLE)
    # OpenGL is deprecated on macOS but still the portable backend.
    target_compile_definitions(OpaaxEngineLib PRIVATE GL_SILENCE_DEPRECATION)
endif()

# A dev build bakes the source-tree root, so paths resolve to the source assets and configs.
# A ship build bakes nothing and resolves everything next to the executable.
if(OPAAX_DEV_BUILD)
    target_compile_definitions(OpaaxEngineLib PUBLIC OPAAX_WORKSPACE_DIR="${CMAKE_SOURCE_DIR}")
    message(STATUS "[Opaax] Dev build: workspace root ${CMAKE_SOURCE_DIR}")
else()
    message(STATUS "[Opaax] Ship build: paths resolve next to the executable")
endif()

target_compile_definitions(OpaaxEngineLib PUBLIC $<$<CONFIG:Debug>:OPAAX_DEBUG>)

# What everything links: the engine library, linked whole. Types register themselves from files
# nothing else references (OPAAX_REGISTER_* macros), and a plain static link would drop those files.
add_library(OpaaxEngine INTERFACE)
target_link_libraries(OpaaxEngine INTERFACE "$<LINK_LIBRARY:WHOLE_ARCHIVE,OpaaxEngineLib>")

# Where the engine's own content lives (shaders, fonts, prototype textures). Games deploy it next
# to their executable in ship builds (see opaax_add_game).
set(OPAAX_ENGINE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets" CACHE INTERNAL "Engine assets folder")

# ==================================
# Unit tests
# ==================================
if(OPAAX_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
