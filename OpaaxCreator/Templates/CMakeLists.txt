# =============================================================================
# __NAME__Module (D9) — the game content unit: a STATIC lib linked by BOTH __NAME__.exe and
# __NAME__Editor.exe. Links Engine only, never an editor header (D4). This is the PRIMARY game
# module — it takes the project's name (Source/__NAME__/, entry __NAME__.h/.cpp). Real game content
# grows here; it is reached transitively through the link, never globbed into an exe.
# =============================================================================
file(GLOB_RECURSE __NAME_UPPER___MODULE_SOURCES CONFIGURE_DEPENDS
    "Source/__NAME__/*.cpp"
    "Source/__NAME__/*.h"
    "Source/__NAME__/*.hpp"
)

add_library(__NAME__Module STATIC ${__NAME_UPPER___MODULE_SOURCES})
target_link_libraries(__NAME__Module PUBLIC OpaaxEngine)
target_include_directories(__NAME__Module PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/__NAME__)

# =============================================================================
# __NAME__.exe (runtime host) — Engine + __NAME__Module. Source/__NAME__Runtime/ holds the thin
# composition (__NAME__App : OpaaxApplication + main). Game content lives in the module
# (Source/__NAME__), reached transitively through the link — never globbed into the exe.
# =============================================================================
file(GLOB_RECURSE __NAME_UPPER___SOURCES CONFIGURE_DEPENDS
    "Source/__NAME__Runtime/*.cpp"
    "Source/__NAME__Runtime/*.h"
)

add_executable(__NAME__ ${__NAME_UPPER___SOURCES})

# Launch from the exe's own dir so the CWD matches where the DLL is post-build-copied.
set_target_properties(__NAME__ PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine + the game module (__NAME__Module PUBLIC-links Engine, but keep Engine explicit
# for clarity — the exe uses engine types directly too).
target_link_libraries(__NAME__ PRIVATE OpaaxEngine __NAME__Module)

target_include_directories(__NAME__ PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/__NAME__Runtime
)

# Copy the engine DLL next to the exe so it runs standalone.
add_custom_command(TARGET __NAME__ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:__NAME__>
    COMMENT "Copying OpaaxEngine.dll to __NAME__ directory..."
)

# =============================================================================
# Project deploy — SHIP BUILDS ONLY.
#
# A dev build bakes OPAAX_WORKSPACE_DIR and resolves against the source tree; a ship build resolves
# from the exe dir, whose deploy must mirror the workspace layout. Since the world's content is
# DATA (Assets/Levels/*.opaaxlevel -> Assets/Maps/*.opaaxmap), a ship build without this finds no
# .opaaxproj, reads an empty startupLevel, and boots into an EMPTY WORLD.
#
# Not copied in dev builds on purpose: nothing reads the exe dir there, and a <exeDir>/__NAME__/
# that looks authoritative but is read by nobody is a decoy that goes stale.
#
# Save/ and Temp/ are NOT deployed: they are runtime output, created on demand.
# =============================================================================
if(NOT OPAAX_EDITOR_SUPPORT)
    set(__NAME___DEPLOY_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/__NAME__)

    add_custom_command(TARGET __NAME__ POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/__NAME__.opaaxproj
            ${__NAME___DEPLOY_DIR}/__NAME__.opaaxproj
        COMMENT "Deploying __NAME__.opaaxproj..."
    )

    # Guarded on EXISTS: an empty directory is not tracked by git, so a fresh clone can legitimately
    # lack one and an unconditional copy_directory would fail the build.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
        add_custom_command(TARGET __NAME__ POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/Assets
                ${__NAME___DEPLOY_DIR}/Assets
            COMMENT "Deploying __NAME__ assets (levels, maps, ...)..."
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Configs)
        add_custom_command(TARGET __NAME__ POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/Configs
                ${__NAME___DEPLOY_DIR}/Configs
            COMMENT "Deploying __NAME__ configs..."
        )
    endif()
endif()

# =============================================================================
# __NAME__Editor.exe (editor host) — Engine + OpaaxEditorLib + __NAME__Module (Editor.md D8).
# Built only under OPAAX_EDITOR_SUPPORT (OpaaxEditorLib exists only then — NOT shipped in release).
# Editor/Source/__NAME__Editor/ holds the thin composition (__NAME__EditorApp + main). Shares
# __NAME__Module with __NAME__.exe; adds the editor via the lib.
# =============================================================================
if(OPAAX_EDITOR_SUPPORT)
    file(GLOB_RECURSE __NAME_UPPER___EDITOR_SOURCES CONFIGURE_DEPENDS
        "Editor/Source/__NAME__Editor/*.cpp"
        "Editor/Source/__NAME__Editor/*.h"
    )

    add_executable(__NAME__Editor ${__NAME_UPPER___EDITOR_SOURCES})

    set_target_properties(__NAME__Editor PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

    target_link_libraries(__NAME__Editor PRIVATE OpaaxEngine OpaaxEditorLib __NAME__Module)

    target_include_directories(__NAME__Editor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Editor/Source/__NAME__Editor
    )

    add_custom_command(TARGET __NAME__Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpaaxEngine>
            $<TARGET_FILE_DIR:__NAME__Editor>
        COMMENT "Copying OpaaxEngine.dll to __NAME__Editor directory..."
    )
endif()
