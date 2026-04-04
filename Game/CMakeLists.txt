file(GLOB_RECURSE GAME_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(Game ${GAME_SOURCES})

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
set(GAME_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/GameAssets)

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GAME_ASSETS_SOURCE}
        ${GAME_ASSETS_DEST}
    COMMENT "Copying game assets..."
)