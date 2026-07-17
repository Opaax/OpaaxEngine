file(GLOB_RECURSE PHYSICSDEMO_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(PhysicsDemo ${PHYSICSDEMO_SOURCES})

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied.
# See Game/CMakeLists.txt for the rationale (relative asset paths miss otherwise from the .sln).
set_target_properties(PhysicsDemo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(PhysicsDemo PRIVATE OpaaxEngine)

# Include dirs
target_include_directories(PhysicsDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET PhysicsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:PhysicsDemo>
    COMMENT "Copying OpaaxEngine.dll to PhysicsDemo directory..."
)

# Copy Asset
set(PHYSICSDEMO_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(PHYSICSDEMO_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/PhysicsDemo/Assets)

add_custom_command(TARGET PhysicsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PHYSICSDEMO_ASSETS_SOURCE}
        ${PHYSICSDEMO_ASSETS_DEST}
    COMMENT "Copying PhysicsDemo assets..."
)

set(PHYSICSDEMO_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${PHYSICSDEMO_MANIFEST_SOURCE})
    message(STATUS "[Opaax] PhysicsDemo manifest not found — generating empty manifest.")
    file(WRITE ${PHYSICSDEMO_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${PHYSICSDEMO_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET PhysicsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PHYSICSDEMO_ASSETS_SOURCE}/AssetManifest.json
        ${PHYSICSDEMO_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying PhysicsDemo asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET PhysicsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsDemo.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/PhysicsDemo/PhysicsDemo.opaaxproj
    COMMENT "Copying PhysicsDemo.opaaxproj..."
)
