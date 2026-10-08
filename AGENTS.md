# Working on OpaaxEngine

Rules and commands for anyone changing this repository, AI agents included. The user docs are in
[Docs/](Docs/); this file is about changing the engine and the editor safely.

## Build and test

| Windows | Linux, macOS | Does |
|---|---|---|
| `build.bat` | `./build.sh` | Configure + build the `debug-editor` preset (engine, editor, games, tests) |
| `build.bat fast <Target>` | `./build.sh fast <Target>` | Rebuild one target without reconfiguring |
| `build.bat test` | `./build.sh test` | Build `OpaaxTests` and run CTest (unit and feature tests) |
| `build.bat release` | `./build.sh release` | Ship build of the games: no editor, content next to the executables |

- The scripts end with `OPAAX_BUILD_OK` or `OPAAX_BUILD_FAIL` and exit 0 or 1: check those, not the
  text. Set `OPAAX_NO_PAUSE=1` so `build.bat` does not wait for a key.
- Windows: never pass `--parallel` to `cmake --build`. MSVC already compiles in parallel (`/MP`), and
  both together exhaust memory.
- Executables: `build/<preset>/bin/<Config>/` with Visual Studio, `build/<preset>/bin/` elsewhere.
- Unit tests directly: `build/debug-editor/bin/Debug/OpaaxTests.exe -tc="Lights*"` (doctest filters).
- Feature tests only: `ctest --test-dir build/debug-editor -C Debug -L feature`; their answers and
  screenshots go to `build/debug-editor/TestWorldResults/`. They open a window: they need a display.
- A new source file is picked up at configure time: run the full `build` once after adding one.
  Test files are listed by hand in `Engine/Tests/CMakeLists.txt`.

## Verify every change

A change is done when it is tested and seen working, not when it compiles.

1. **Unit tests** for any logic: `Engine/Tests/<Module>/<Thing>Tests.cpp` (see
   [Engine/Tests/README.md](Engine/Tests/README.md)). Make the logic testable without a GPU or a window:
   pure functions in headers, the GPU part kept thin.
2. **Feature tests** for anything a game sees: a level in `TestWorld/Assets` and a script in
   `TestWorld/Tests/<Feature>.json` that plays it and checks values with `expect.*`. Probes are
   behaviours that record what happened in their fields (`TestWorld/Source/TestWorld/Probes`).
3. **Look at it** when it draws: run a script with a `screenshot` request, or
   `Sandbox --capture shot.png --capture-frame 30`, then open the PNG. Do not describe a picture you
   have not looked at.
4. **The editor** is checked by driving it: `SandboxEditor --exec script.json` with the editor's
   commands (create entities, edit components, undo, play) and a screenshot; a script in
   `TestWorld/EditorTests` runs in CTest. See [Docs/Automation.md](Docs/Automation.md).
5. **CI** (`.github/workflows/build.yml`): Windows, Ubuntu and macOS, debug-editor and release.
   Linux also renders on Mesa's software OpenGL (stricter than desktop drivers) and runs the feature
   tests there. A change is finished when every job is green.

## Code style

Match the code around you. In short:

- Allman braces, 4 spaces, no tabs. One class per header/source pair when it has code.
- Names: `m_Member`, `InParam`, `OutParam`, `lLocal`, `bBool` (and `bInFlag` for a bool parameter),
  `TTemplate`, `EEnum`, `IInterface`, `UPPER_CASE` constants. Types and functions in PascalCase.
- Engine aliases: `TDynArray`, `TUnorderedMap`, `TUnorderedSet`, `TUniquePtr`/`MakeUnique`,
  `TSharedPtr`/`MakeShared`, `TFunction`, `TFixedArray`, `Move`, `Int32`, `Uint64`...
  (`Core/OpaaxTypes.h`); `OpaaxString`, `OpaaxStringID` (`OPAAX_ID("Name")`).
- Logging: `OPAAX_LOG_CATEGORY(Name)` once, then `OPAAX_LOG(LogName, Info, "x = {}", lX)`. Levels:
  Trace, Info, Warn, Error, Critical. CI fails a render check that logs an error.
- Sections are separated by `// ===` banners; interface overrides by `//~Begin X interface` and
  `//~End X interface`.
- Comments are short and plain: what a block is for, or why it is done this way. No change history,
  no plan or ticket ids, no "fixed", "now", "new" in comments: that belongs in the commit message.
- Every public function and type gets a one- or two-line `/** */` comment when its name does not say
  everything.

## Architecture rules

- **Third-party APIs only behind their wrapper:** EnTT in `World/`, GLFW in `Platform/`, Box2D in
  `Physics/Box2D/`, miniaudio in `Audio/`, OpenGL in `RHI/OpenGL/`, stb in the texture loader and
  the font baker, tinyfiledialogs in the editor's dialogs. Type ids come from
  `Core/Reflection/TypeInfo.h`. Gameplay code uses `Entity`, `World`, `Behaviour`: never the registry.
- **Types register themselves** with `OPAAX_REGISTER_*` (`Engine/Registries/AutoRegistration.h`):
  no central list to edit.
- The engine is a static library, linked whole; it knows nothing of the editor. The editor library
  depends on the engine; games depend on the engine (and their editor module on the editor).
- Rendering is OpenGL 4.1 everywhere (macOS): shaders are GLSL 4.50 with explicit bindings, ported
  at load time. 16 samplers per stage. Uniform blocks are std140 with `Vector4F` members only, and
  their C++ mirrors carry a `static_assert` on their size.
- Content goes through `IPaths` (asset-relative paths, `/Engine/...` for engine content), never
  hard-coded folders.
- No global state beyond the application's service locator; systems get what they need from their
  context (`WorldContext`, `EditorContext`).

## Adding something

- **Component or behaviour:** the type with `OPAAX_PROPERTIES` (and the JSON macro for a component),
  one `OPAAX_REGISTER_*` line, unit tests for its logic, a TestWorld probe or level when a game can
  see it. Old maps must still load: new fields have defaults (`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`).
- **Renderer feature:** the CPU side (packing, layout, limits) as tested pure functions; the shader in
  `Engine/Assets/Shaders` with its bindings checked in `EngineShaderTests`; a TestWorld level and a
  screenshot looked at.
- **Editor feature:** a command (`Editor/Commands`), registered and reachable by tag; edits undoable
  through `EntityOps` or an undo step (`Editor/Undo`); an automation command when an agent should be
  able to use it; driven once through automation before committing.
- **Automation command:** registered with a one-line help; it fails with a message that says what to
  do instead; a long job answers when done (`AutomationResult::Answer`). Document it in
  `Docs/Automation.md`.

## Commits

- One logical change per commit, in small steps, each with its test. Review the whole diff before
  committing: style, comments, leftovers, debug logs.
- Message: `[Tag] What changed` (tags: `Feat`, `Fix`, `Refactor`, `Test`, `Docs`, `CI`, `Build`),
  then a body saying why and how it was checked.
- Never commit build outputs, `Save/` folders or local editor state.

## Known traps

- Windows shells: heredocs mangle backslashes and non-ASCII text, and Python's `write_text` writes CRLF.
  Edit files with an editor or byte-level scripts. Batch files are CRLF in the working tree,
  everything else LF (`.gitattributes`).
- A shader that compiles on a desktop driver can fail on Mesa or macOS: keep to GLSL 4.10 features
  (no `binding` in the ported source, constant sampler-array indices).
- `--capture` and automation screenshots read the back buffer: in the editor they show the whole
  editor, not only the viewport.
- Physics bodies of entities spawned this frame are built on first use; behaviours start at the next
  drain point, so an event sent in `OnStart` to a not-yet-started entity starts it first.
