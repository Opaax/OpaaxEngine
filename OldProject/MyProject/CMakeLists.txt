
file(GLOB_RECURSE MYPROJECT_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

add_executable(MyProject ${MYPROJECT_SOURCES})

set_target_properties(MyProject PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(MyProject PRIVATE OpaaxEngine)

# Include dirs
target_include_directories(MyProject PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET MyProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:MyProject>
    COMMENT "Copying OpaaxEngine.dll to MyProject directory..."
)

# Copy Asset
set(MYPROJECT_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(MYPROJECT_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/MyProject/Assets)

add_custom_command(TARGET MyProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MYPROJECT_ASSETS_SOURCE}
        ${MYPROJECT_ASSETS_DEST}
    COMMENT "Copying MyProject assets..."
)

set(MYPROJECT_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${MYPROJECT_MANIFEST_SOURCE})
    message(STATUS "[Opaax] MyProject manifest not found — generating empty manifest.")
    file(WRITE ${MYPROJECT_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${MyProject_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET MyProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${MYPROJECT_ASSETS_SOURCE}/AssetManifest.json
        ${MYPROJECT_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying MyProject asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET MyProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/MyProject.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/MyProject/MyProject.opaaxproj
    COMMENT "Copying MyProject.opaaxproj..."
)


