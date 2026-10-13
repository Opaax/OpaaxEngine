# Lessons

What working on this repository taught, kept short so the next session starts from it.

## Tools and shell

- The file-writing tools turn a `\uXXXX` sequence (four hex digits) into its character. A file that
  must keep such an escape (a C++ `u8` literal in the UTF-8 tests) is edited around it: match or
  cut by a nearby line, never by the escaped text.
- MSVC's "compiler is out of heap space" (C1060) during `build.bat` is memory pressure, not the code:
  build again before changing anything.
- CI cancels a branch's running workflow when a newer commit is pushed: push the last commit, then
  watch that run (`Scratch/tools/ci_watch.py <sha>`).

## Code

- A header shared by the tests stays minimal: what it includes reaches every suite under
  `using namespace Opaax` (TestTempDir.h pulled `Clock` in and clashed with a test's `Clock`). Put
  the body in a source file.
- When a standard header leaves a widely included header (as `<filesystem>` left `OpaaxTypes.h`), the
  full build on Windows proves little: GCC and Clang include less transitively, so wait for CI.
- A CMake script that reads source lines must set aside `;`, `[`, `]` and `\` before splitting the
  text into a list, or the line numbers drift (`CMake/CheckStdUse.cmake`).

## Looking at the editor

- The editor's layout is local (`TestWorld/Editor/Save/imgui.ini`) and can hide a panel in a
  screenshot. For a check, copy it aside, give the run a small layout of its own, and put the user's
  file back afterwards.
