---
name: opaax-build-and-test
description: Build OpaaxEngine and run its unit tests and TestWorld feature tests, and read the failures. Use when compiling the engine, editor or a game, when running or adding tests, or when checking a change before a commit.
---

# Build and test OpaaxEngine

## Build

- Full build (configure + build, debug with the editor): `build.bat` on Windows, `./build.sh` on Linux
  and macOS. The last line is `OPAAX_BUILD_OK` or `OPAAX_BUILD_FAIL`; the exit code matches.
  Set `OPAAX_NO_PAUSE=1` on Windows.
- One target, no reconfigure: `build.bat fast OpaaxTests` (or `Sandbox`, `TestWorld`, `SandboxEditor`...).
- Direct CMake: `cmake --preset debug-editor` then
  `cmake --build build/debug-editor --config Debug [--target X]`. On Windows never add `--parallel`
  (MSVC builds in parallel already; both together run out of memory).
- Added a source file? Configure again (`cmake --preset debug-editor`): sources are globbed at
  configure time. Test files must also be listed in `Engine/Tests/CMakeLists.txt`.
- Find errors in a long log: `grep -E " error |warning C" build.log` (MSVC), `grep -E "error:|warning:"`
  (GCC, Clang).

## Unit tests

- All: `build/debug-editor/bin/Debug/OpaaxTests.exe` (Windows) or `build/debug-editor/bin/OpaaxTests`.
  The summary line gives cases and assertions; failures print the file, line and both values.
- A subset: `-tc="Lights*"` (case names), `-sf="*Lighting2D*"` (source files), `-ts=<suite>`.
- Writing one: `Engine/Tests/<Module>/<Thing>Tests.cpp`, doctest (`TEST_CASE`, `CHECK`, `REQUIRE`,
  `doctest::Approx` for floats), registered in `Engine/Tests/CMakeLists.txt`. Engine code is reachable
  as from a game. No GPU and no window: test pure functions, keep GPU code thin. Header-only editor
  types can be tested too (`Engine/Tests/Editor`).
- Prove a test catches the bug: break the code (or the data it reads) once, see it fail, restore.

## Feature tests (TestWorld)

- `ctest --test-dir build/debug-editor -C Debug -L feature --output-on-failure`. Each runs
  `TestWorld --exec TestWorld/Tests/<Feature>.json`; answers and screenshots land in
  `build/debug-editor/TestWorldResults/`.
- A failure: open `<Feature>.out.json` and find the response with `"ok": false`; its `error` says what
  was expected and what was found. Look at `<Feature>.png`.
- They need a display (a window opens). CI runs them on Linux under `xvfb-run` with Mesa.

## Before committing

1. Build, all unit tests, the feature tests touched by the change: green.
2. Anything visible: a screenshot looked at.
3. Review the diff (style, comments, leftovers), commit with `[Tag] summary` and a body.
4. Push and check CI (6 jobs: Windows, Ubuntu, macOS x debug-editor, release). The GitHub API
   (`/repos/<owner>/<repo>/actions/runs?head_sha=<sha>`) gives the jobs and failing steps.
