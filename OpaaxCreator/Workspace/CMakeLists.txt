# =============================================================================
# __WORKSPACE__ — a game repository on the Opaax engine (Docs/GameRepository.md in the engine).
#   The engine is added as a subdirectory, then each game: a folder with a CMakeLists.txt calling
#   opaax_add_game(). MakeOpaax adds a game's line at the end of this file.
# =============================================================================
cmake_minimum_required(VERSION 3.24)
project(__WORKSPACE__ LANGUAGES C CXX)

# Executables in build/<preset>/bin, as in the engine's repository.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# CTest finds the tests from the top of the build tree.
enable_testing()

# The engine, its editor and the functions that declare a game. Its own games and unit tests are
# left out (OPAAX_BUILD_SAMPLES, OPAAX_BUILD_ENGINE_TESTS).
add_subdirectory("__ENGINE_PATH__" OpaaxEngine)
