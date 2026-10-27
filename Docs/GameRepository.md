# A game in its own repository

A game can live in a repository of its own, with the engine as a git submodule: the game keeps its
history, issues and CI, and takes a new engine when it chooses to. The engine's repository holds its
own games the same way (Sandbox, TestWorld), so the rest of these docs applies as it is.

## Layout

```
MyGame/                          the game repository: the top-level CMake project, the workspace
  CMakeLists.txt                 adds the engine, then each game
  CMakePresets.json              the engine's presets: debug-editor, debug, release
  External/OpaaxEngine/          the engine, a git submodule
  MyGame/                        the game, as MakeOpaax makes it (QuickStart.md)
    MyGame.opaaxproj
    Assets/  Configs/  Source/  Editor/  Tests/
```

A workspace holds one folder per game. A dev build finds a game at
`<workspace>/<Game>/<Game>.opaaxproj`, and the engine's content in the engine's own folder, wherever
the workspace keeps it; `app.info` answers both.

## Create it

```
git init MyGame
cd MyGame
git submodule add https://github.com/Opaax/OpaaxEngine.git External/OpaaxEngine
git submodule update --init --recursive
External\OpaaxEngine\MakeOpaax MyGame .
```

`MakeOpaax <Name> <Folder>` (`./MakeOpaax.sh` on Linux and macOS) creates the game in
`<Folder>/<Name>` and adds it to `<Folder>/CMakeLists.txt`. A folder without a `CMakeLists.txt` yet
gets one that adds the engine by its path from the folder, with the presets and a `.gitignore`
(`OpaaxCreator/Workspace` in the engine); a file already there is kept. A second game is another
`MakeOpaax` with the same folder. Give the folder every time: without one, the game goes into the
engine's own checkout.

The two files, written by hand:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyGame LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
enable_testing()
add_subdirectory(External/OpaaxEngine OpaaxEngine)
add_subdirectory(MyGame)
```

```json
{ "version": 4, "include": [ "External/OpaaxEngine/CMakePresets.json" ] }
```

## Build, run, test

```
cmake --preset debug-editor
cmake --build build/debug-editor --config Debug
ctest --test-dir build/debug-editor -C Debug
```

The editor is `build/debug-editor/bin/Debug/MyGameEditor` (`bin/` without Visual Studio). `release`
is the ship build; **File > Export Game...** works as in the engine's repository. What is built: the
engine, its editor and the repository's games. The engine's own games and unit tests are left out:

| Option | In a game repository | Builds |
|---|---|---|
| `OPAAX_BUILD_SAMPLES` | OFF | Sandbox, TestWorld |
| `OPAAX_BUILD_ENGINE_TESTS` | OFF | OpaaxTests and the standard library check |
| `OPAAX_BUILD_TESTS` | ON | the tests, the games' included |

The first two are on when the engine is the top-level project. Turn them on
(`cmake --preset debug-editor -DOPAAX_BUILD_ENGINE_TESTS=ON`) to check the engine itself from the
game's repository; OpaaxTests then writes its run's files in `OpaaxTests/` at the top of the
repository, which the `.gitignore` MakeOpaax writes leaves out.

## Tests

A game made by MakeOpaax has both kinds, declared after the game in its `CMakeLists.txt`:

```cmake
opaax_add_game(MyGame)

opaax_add_game_tests(MyGame
    Tests/Unit/MyGameTests.cpp
    Tests/Unit/ScoreTests.cpp)

opaax_add_game_feature_tests(MyGame Tests)
```

- `opaax_add_game_tests(<Name> <sources...>)`: doctest cases in one executable, `<Name>Tests`, which
  links the game module whole as the game does, so every type the module registers is there. CTest
  runs it as `<Name>Tests`. How to write them: [Engine/Tests/README.md](../Engine/Tests/README.md).
- `opaax_add_game_feature_tests(<Name> <dir>)`: a test per script `<dir>/*.json`, named
  `<Name>.<Script>`: the game's runtime plays it and checks it with `expect.*`
  ([Automation.md](Automation.md)). The answers and screenshots go to `build/<preset>/<Name>Results`.
  They open a window, so they are labelled `feature` (`ctest -LE feature` leaves them out).

## Continuous integration

The engine's CI helpers work from a game repository:

| Helper | Does |
|---|---|
| `External/OpaaxEngine/.github/scripts/report_errors.py <log>` | A build log's compiler and linker errors as annotations |
| `External/OpaaxEngine/.github/problem-matchers/doctest.json` | Failed doctest checks as annotations |
| `External/OpaaxEngine/.github/scripts/report_feature_failures.py <build> <game folder>` | Each failed request of the feature tests (the scripts of `<game folder>/Tests`) as an annotation |

A workflow (`.github/workflows/build.yml`) that builds and tests the game on Windows and Linux; the
feature tests need a screen, a virtual one on Linux. The engine's own CI runs the same steps on a
game repository that MakeOpaax makes (its `game-repository` job).

```yaml
name: build
on: [push, workflow_dispatch]

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        os: [windows-latest, ubuntu-24.04]
    runs-on: ${{ matrix.os }}
    defaults:
      run:
        shell: bash
    steps:
      - uses: actions/checkout@v5
        with:
          submodules: recursive

      - name: Linux packages
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \
            libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
            libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev \
            libgl1-mesa-dev xvfb libgl1-mesa-dri

      - name: Configure
        run: cmake --preset debug-editor 2>&1 | tee configure.log

      # MSVC compiles a project's files in parallel already; elsewhere the job count is given.
      - name: Build
        run: |
          if [ "$RUNNER_OS" = "Windows" ]; then JOBS=""; else JOBS="--parallel 4"; fi
          cmake --build build/debug-editor --config Debug $JOBS 2>&1 | tee build.log

      - name: Report errors
        if: failure()
        run: |
          python3 External/OpaaxEngine/.github/scripts/report_errors.py configure.log || python External/OpaaxEngine/.github/scripts/report_errors.py configure.log
          python3 External/OpaaxEngine/.github/scripts/report_errors.py build.log || python External/OpaaxEngine/.github/scripts/report_errors.py build.log

      - name: Unit tests
        run: |
          echo "::add-matcher::External/OpaaxEngine/.github/problem-matchers/doctest.json"
          ctest --test-dir build/debug-editor -C Debug --output-on-failure -LE feature

      - name: Feature tests
        if: runner.os == 'Linux'
        run: xvfb-run -a -s "-screen 0 1280x720x24" ctest --test-dir build/debug-editor -C Debug -L feature --output-on-failure

      - name: Report feature failures
        if: runner.os == 'Linux' && failure()
        run: python3 External/OpaaxEngine/.github/scripts/report_feature_failures.py build/debug-editor MyGame
```

## A new engine

```
cd External/OpaaxEngine
git fetch
git checkout <tag or commit>
cd ../..
git add External/OpaaxEngine
git commit -m "Engine: <tag or commit>"
```

Then build again. Asset files of a format the new engine no longer loads are converted by starting
the game's editor once with `--upgrade-assets` ([Objects.md](Objects.md)).
