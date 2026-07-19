# =============================================================================
# SandboxModule (D9) — the game content unit, a STATIC lib linked by BOTH Sandbox.exe and
# (from S8) SandboxEditor.exe. Links Engine only, never an editor header. Empty at S1;
# content moves in at S2, RegisterModule at S6. Globbed from Module/ (kept OUT of the exe's
# own Source/ glob below).
# =============================================================================
file(GLOB_RECURSE SANDBOX_MODULE_SOURCES CONFIGURE_DEPENDS
    "Module/*.cpp"
    "Module/*.h"
    "Module/*.hpp"
)

add_library(SandboxModule STATIC ${SANDBOX_MODULE_SOURCES})
target_link_libraries(SandboxModule PUBLIC OpaaxEngine)
target_include_directories(SandboxModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Module)

# =============================================================================
# Sandbox.exe (runtime) — Engine + SandboxModule. Runtime/ holds the runtime app
# (SandboxApp : OpaaxApplication) + main. Game content lives in Module/ (SandboxModule),
# reached transitively through the link — never globbed into the exe.
# =============================================================================
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    "Runtime/*.cpp"
    "Runtime/*.h"
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

# Include dirs — Runtime/ for the app headers. Engine Source is PUBLIC via OpaaxEngine;
# Module/ (Config, future components) is PUBLIC via SandboxModule — both propagate through the link.
target_include_directories(Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Runtime
)

# Copy the engine DLL next to the exe so it runs standalone.
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:Sandbox>
    COMMENT "Copying OpaaxEngine.dll to Sandbox directory..."
)

# =============================================================================
# SandboxEditor.exe (editor host) — Engine + OpaaxEditorLib + SandboxModule (Editor.md D8). Built only
# under OPAAX_EDITOR_SUPPORT (OpaaxEditorLib exists only then). EditorMain/ holds the ~thin composition
# (SandboxEditorApp + main). Shares SandboxModule with Sandbox.exe; adds the editor via the lib.
# =============================================================================
if(OPAAX_EDITOR_SUPPORT)
    file(GLOB_RECURSE SANDBOX_EDITOR_SOURCES CONFIGURE_DEPENDS
        "EditorMain/*.cpp"
        "EditorMain/*.h"
    )

    add_executable(SandboxEditor ${SANDBOX_EDITOR_SOURCES})

    set_target_properties(SandboxEditor PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

    target_link_libraries(SandboxEditor PRIVATE OpaaxEngine OpaaxEditorLib SandboxModule)

    target_include_directories(SandboxEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/EditorMain
    )

    add_custom_command(TARGET SandboxEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpaaxEngine>
            $<TARGET_FILE_DIR:SandboxEditor>
        COMMENT "Copying OpaaxEngine.dll to SandboxEditor directory..."
    )
endif()
