# =============================================================================
# SandboxModule (D9) — the game content unit: a STATIC lib linked by BOTH Sandbox.exe and
# SandboxEditor.exe. Links Engine only, never an editor header (D4). This is the PRIMARY game
# module — it takes the project's name (Source/Sandbox/, entry Sandbox.h/.cpp). Real game content
# grows here; it is reached transitively through the link, never globbed into an exe.
# =============================================================================
file(GLOB_RECURSE SANDBOX_MODULE_SOURCES CONFIGURE_DEPENDS
    "Source/Sandbox/*.cpp"
    "Source/Sandbox/*.h"
    "Source/Sandbox/*.hpp"
)

add_library(SandboxModule STATIC ${SANDBOX_MODULE_SOURCES})
target_link_libraries(SandboxModule PUBLIC OpaaxEngine)
target_include_directories(SandboxModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Sandbox)

# =============================================================================
# Sandbox.exe (runtime host) — Engine + SandboxModule. Source/SandboxRuntime/ holds the thin
# composition (SandboxApp : OpaaxApplication + main). Game content lives in the module (Source/Sandbox),
# reached transitively through the link — never globbed into the exe.
# =============================================================================
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    "Source/SandboxRuntime/*.cpp"
    "Source/SandboxRuntime/*.h"
)

# Exe
add_executable(Sandbox ${SANDBOX_SOURCES})

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied
# (mirrors PhysicsDemo/Game — relative asset paths miss otherwise when run from the .sln).
set_target_properties(Sandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine + the game module (SandboxModule PUBLIC-links Engine, but keep Engine explicit
# for clarity — the exe uses engine types directly too).
target_link_libraries(Sandbox PRIVATE OpaaxEngine SandboxModule)

# Include dirs — SandboxRuntime/ for the app headers. Engine Source is PUBLIC via OpaaxEngine;
# the module dir (Source/Sandbox: Config, future components) is PUBLIC via SandboxModule — both
# propagate through the link.
target_include_directories(Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/SandboxRuntime
)

# Copy the engine DLL next to the exe so it runs standalone.
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:Sandbox>
    COMMENT "Copying OpaaxEngine.dll to Sandbox directory..."
)

# =============================================================================
# Project deploy — SHIP BUILDS ONLY.
#
# Engine/CMakeLists.txt has stated the contract for a while: a dev build bakes OPAAX_WORKSPACE_DIR
# and resolves against the source tree, while a ship build "resolves from the exe dir, WHOSE DEPLOY
# MIRRORS THE WORKSPACE LAYOUT". That deploy was never actually written, which cost nothing until
# M5 — the world used to be spawned in C++, so a ship build with no project files still came up
# looking right. Now the content IS the files: without this, Sandbox.exe finds no .opaaxproj, reads
# an empty startupLevel, and boots into an EMPTY WORLD.
#
# Gated on the ship build rather than copied always, deliberately. In a dev build nothing reads the
# exe dir, so an unconditional copy would leave a <exeDir>/Sandbox/ that looks authoritative, is
# read by nobody, and goes stale — exactly the decoy that costs an hour the next time someone
# debugs a path.
#
# Save/ and Temp/ are NOT deployed: they are runtime output, created on demand.
# =============================================================================
if(NOT OPAAX_EDITOR_SUPPORT)
    set(SANDBOX_DEPLOY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/Sandbox)

    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Sandbox.opaaxproj
            ${SANDBOX_DEPLOY_DIR}/Sandbox.opaaxproj
        COMMENT "Deploying Sandbox.opaaxproj..."
    )

    # Guarded on EXISTS for the same reason the engine's asset copies are: an empty directory is
    # not tracked by git, so a fresh clone can legitimately lack one and an unconditional
    # copy_directory would fail the build with "cannot find the path specified".
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
        add_custom_command(TARGET Sandbox POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/Assets
                ${SANDBOX_DEPLOY_DIR}/Assets
            COMMENT "Deploying Sandbox assets (levels, maps, ...)..."
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Configs)
        add_custom_command(TARGET Sandbox POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/Configs
                ${SANDBOX_DEPLOY_DIR}/Configs
            COMMENT "Deploying Sandbox configs..."
        )
    endif()
endif()

# =============================================================================
# SandboxEditor.exe (editor host) — Engine + OpaaxEditorLib + SandboxModule (Editor.md D8). Built only
# under OPAAX_EDITOR_SUPPORT (OpaaxEditorLib exists only then, and this whole Editor/ tree is
# editor-only — NOT shipped in release). Editor/Source/SandboxEditor/ holds the ~thin composition
# (SandboxEditorApp + main). Shares SandboxModule with Sandbox.exe; adds the editor via the lib.
# =============================================================================
if(OPAAX_EDITOR_SUPPORT)
    file(GLOB_RECURSE SANDBOX_EDITOR_SOURCES CONFIGURE_DEPENDS
        "Editor/Source/SandboxEditor/*.cpp"
        "Editor/Source/SandboxEditor/*.h"
    )

    add_executable(SandboxEditor ${SANDBOX_EDITOR_SOURCES})

    set_target_properties(SandboxEditor PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

    target_link_libraries(SandboxEditor PRIVATE OpaaxEngine OpaaxEditorLib SandboxModule)

    target_include_directories(SandboxEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Editor/Source/SandboxEditor
    )

    add_custom_command(TARGET SandboxEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpaaxEngine>
            $<TARGET_FILE_DIR:SandboxEditor>
        COMMENT "Copying OpaaxEngine.dll to SandboxEditor directory..."
    )
endif()
