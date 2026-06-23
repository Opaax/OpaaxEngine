file(GLOB_RECURSE SANDBOX_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(Sandbox ${SANDBOX_SOURCES}
        Source/Sandbox.h)

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied
# (mirrors PhysicsDemo/Game — relative asset paths miss otherwise when run from the .sln).
set_target_properties(Sandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(Sandbox PRIVATE OpaaxEngine)

# Include dirs (engine Source is propagated PUBLIC by the OpaaxEngine target)
target_include_directories(Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Copy the engine DLL next to the exe so it runs standalone.
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:Sandbox>
    COMMENT "Copying OpaaxEngine.dll to Sandbox directory..."
)
