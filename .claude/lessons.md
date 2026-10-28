# Lessons

What working on this repository taught, kept short so the next session starts from it.

## Tools and shell

- The file-writing tools turn a `\uXXXX` sequence (four hex digits) into its character. A file that
  must keep such an escape (a C++ `u8` literal in the UTF-8 tests) is edited around it: match or
  cut by a nearby line, never by the escaped text.
- MSVC's "compiler is out of heap space" (C1060) during `build.bat` is memory pressure, not the code:
  build again before changing anything.
- Run `build.bat` from the PowerShell tool as `& .\build.bat fast OpaaxTests` (with
  `$env:OPAAX_NO_PAUSE = '1'`): `cmd /c build.bat` from either shell answers "not recognized".
- A Python edit script with `'''` strings breaks the Bash tool's heredoc ("unexpected EOF"): write the
  script to the scratchpad and run it with `python -I <file>`.
- CI cancels a branch's running workflow when a newer commit is pushed: push the last commit, then
  watch that run (`Scratch/tools/ci_watch.py <sha>`).
- A full build once hung on one `cl.exe` (log stopped, CPU flat for minutes): stop the build's
  processes and `mspdbsrv.exe`, then build again in the background.
- No Clang compiler is installed, but Visual Studio's `clang-tidy` is Clang's front end: with a Ninja
  compile db (`Scratch/clangcheck/configure.bat`), `Scratch/clangcheck/run_tidy.py` compiles every own
  file as Clang does in about two minutes, and reproduced the macOS CI errors exactly. Run it before
  pushing; ImGui's `static_assert` errors there are clang-cl noise.
- A failed CI job's log needs admin rights through the API, but its annotations (the compiler errors)
  are public: `api.github.com/repos/Opaax/OpaaxEngine/check-runs/<job id>/annotations`.
- Bash calls sent together share one shell: the `cd` of one moved the other, whose `cmake -S . -B build`
  then configured a CMake tree in the repository's `build/` itself. Give CMake absolute `-S` and `-B`
  paths, and never count on a `cd` when calls run side by side.

## Code

- A header shared by the tests stays minimal: what it includes reaches every suite under
  `using namespace Opaax` (TestTempDir.h pulled `Clock` in and clashed with a test's `Clock`). Put
  the body in a source file.
- When a standard header leaves a widely included header (as `<filesystem>` left `OpaaxTypes.h`), the
  full build on Windows proves little: GCC and Clang include less transitively, so wait for CI.
- A CMake script that reads source lines must set aside `;`, `[`, `]` and `\` before splitting the
  text into a list, or the line numbers drift (`CMake/CheckStdUse.cmake`).
- A `TAssetRef<T>` field is visited through `T::StaticClass()`: wherever the owner registers or draws
  its properties, `T` must be complete (the editor's NativeComponentDrawers.cpp needed the asset
  headers of the components it draws).
- `/Engine/...` is an absolute path on Linux and macOS: test the engine mount before `IsAbsolute`.
- Clang refuses `const T x;` when T or one of its members has a defaulted default constructor and an
  anonymous union with an uninitialized member; MSVC and GCC accept it. OpaaxString had one, so every
  type holding a string was caught: its constructor is written out (`constexpr OpaaxString() noexcept {}`).
- A test that a module's type is registered proves nothing about linking the module whole when the
  test includes the type's header: a registration in a header runs in the test's own file. Check from
  a file that does not include it; linked plainly, that check fails.

## Running the games

- A game run with `--exec` writes a screenshot's relative path into its working directory: run it from
  `build/debug-editor/TestWorldResults`, as CTest does, or a run from the repository root leaves PNGs there.
- A map written by hand (Python's `json`) re-serializes differently (floats as typed, not as the engine
  writes them), and the editor warns on open. Resave it once: `level.open`, then `level.save` with `all`.

## Looking at the editor

- The editor's layout is local (`TestWorld/Editor/Save/imgui.ini`) and can hide a panel in a
  screenshot. For a check, copy it aside, give the run a small layout of its own, and put the user's
  file back afterwards.
- A window restored from that layout draws on its second frame: `frames.wait` before a screenshot.
- Automation cannot click inside a window. A custom asset editor's selected view is checked with a
  local build that starts with a selection, then reverted.
- An automation step checking a `PostEditChange` fix-up must end on values that differ from the
  file: a fix-up that gives back the file's own values leaves the asset clean (`unsaved` false).
