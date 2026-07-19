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
