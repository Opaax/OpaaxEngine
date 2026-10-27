# The __NAME__ game: module, runtime and editor executables (see CMake/OpaaxGame.cmake).
opaax_add_game(__NAME__)

# Unit tests: the files of Tests/Unit, listed here, in one executable (__NAME__Tests).
opaax_add_game_tests(__NAME__
    Tests/Unit/__NAME__Tests.cpp)

# Feature tests: each script in Tests/ plays the game and checks it (__NAME__.<Script>).
opaax_add_game_feature_tests(__NAME__ Tests)
