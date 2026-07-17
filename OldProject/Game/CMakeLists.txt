file(GLOB_RECURSE GAME_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(Game ${GAME_SOURCES})

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied
# (bin/<config>/{Engine,Game}/Assets). Without this, F5/Ctrl+F5 from the .sln runs with
# CWD = the project build dir and the relative asset paths (Sprite.glsl, etc.) miss.
set_target_properties(Game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY 
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(Game PRIVATE OpaaxEngine)

# Include dirs
target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:Game>
    COMMENT "Copying OpaaxEngine.dll to game directory..."
)

# Copy Asset
set(GAME_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(GAME_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/Game/Assets)

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GAME_ASSETS_SOURCE}
        ${GAME_ASSETS_DEST}
    COMMENT "Copying game assets..."
)

set(GAME_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${GAME_MANIFEST_SOURCE})
    message(STATUS "[Opaax] Game manifest not found — generating empty manifest.")
    file(WRITE ${GAME_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${GAME_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GAME_ASSETS_SOURCE}/AssetManifest.json
        ${GAME_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying game asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Game.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/Game/Game.opaaxproj
    COMMENT "Copying Game.opaaxproj..."
)