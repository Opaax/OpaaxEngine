file(GLOB_RECURSE SIMPLESHMUP_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(SimpleShmup ${SIMPLESHMUP_SOURCES})

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied.
# See Game/CMakeLists.txt for the rationale (relative asset paths miss otherwise from the .sln).
set_target_properties(SimpleShmup PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(SimpleShmup PRIVATE OpaaxEngine)

# Include dirs
target_include_directories(SimpleShmup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET SimpleShmup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:SimpleShmup>
    COMMENT "Copying OpaaxEngine.dll to SimpleShmup directory..."
)

# Copy Asset
set(SIMPLESHMUP_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(SIMPLESHMUP_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/SimpleShmup/Assets)

add_custom_command(TARGET SimpleShmup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SIMPLESHMUP_ASSETS_SOURCE}
        ${SIMPLESHMUP_ASSETS_DEST}
    COMMENT "Copying SimpleShmup assets..."
)

set(SIMPLESHMUP_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${SIMPLESHMUP_MANIFEST_SOURCE})
    message(STATUS "[Opaax] SimpleShmup manifest not found — generating empty manifest.")
    file(WRITE ${SIMPLESHMUP_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${SIMPLESHMUP_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET SimpleShmup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SIMPLESHMUP_ASSETS_SOURCE}/AssetManifest.json
        ${SIMPLESHMUP_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying SimpleShmup asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET SimpleShmup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/SimpleShmup.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/SimpleShmup/SimpleShmup.opaaxproj
    COMMENT "Copying SimpleShmup.opaaxproj..."
)
