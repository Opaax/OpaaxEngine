file(GLOB_RECURSE GAME_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Créer l'exécutable du jeu
add_executable(Game ${GAME_SOURCES})

# Lier avec le moteur
target_link_libraries(Game PRIVATE OpaaxEngine)

# Include directories
target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# Copier la DLL du moteur à côté de l'exe
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:Game>
    COMMENT "Copying OpaaxEngine.dll to game directory..."
)

# Copier les assets du jeu
set(GAME_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(GAME_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/GameAssets)

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GAME_ASSETS_SOURCE}
        ${GAME_ASSETS_DEST}
    COMMENT "Copying game assets..."
)