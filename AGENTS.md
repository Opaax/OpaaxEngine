# Working on OpaaxEngine

Rules and commands for anyone changing this repository, AI agents included. The user docs are in
[Docs/](Docs/); this file is about changing the engine and the editor safely. How the parts fit,
and why they are built that way: [Docs/Architecture.md](Docs/Architecture.md).

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
- The standard library check alone: `cmake -P CMake/CheckStdUse.cmake` (CTest runs it as `StdUse`);
  it lists each std use outside the wrapper files as `file:line: text`.
- Work stays in the repository: a trial checkout, its build, a test project or a tool kept between
  sessions goes in `Scratch/`, which git ignores (a build under the system's temp folder misses header
  changes). Throwaway files (edit lists, logs, screenshots) may go to a temp folder. Never use `.claude/`
  for either: every write there asks for approval.

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
   commands (create entities, edit components and assets, undo, play) and a screenshot; a script in
   `TestWorld/EditorTests` runs in CTest. See [Docs/Automation.md](Docs/Automation.md). Automation
   cannot click inside a window: what a custom asset editor shows for a selection is checked with a
   local build that starts selected, then reverted.
5. **CI** (`.github/workflows/build.yml`): Windows, Ubuntu and macOS, debug-editor and release.
   Linux also renders on Mesa's software OpenGL (stricter than desktop drivers) and runs the feature
   tests there. A change is finished when every job is green.

## Code style

Match the code around you. In short:

- Allman braces, 4 spaces, no tabs. One class per header/source pair when it has code.
- Names: `m_Member`, `InParam`, `OutParam`, `lLocal`, `bBool` (and `bInFlag` for a bool parameter),
  `TTemplate`, `EEnum`, `IInterface`, `UPPER_CASE` constants. Types and functions in PascalCase.
- No `std::` and no standard header outside Core's wrappers (see the architecture rules). Engine
  aliases: `TDynArray`, `TUnorderedMap`, `TUnorderedSet`, `TUniquePtr`/`MakeUnique`,
  `TSharedPtr`/`MakeShared`, `TFunction`, `TFixedArray`, `TOptional`, `Move`, `Int32`, `Uint64`...
  (`Core/OpaaxTypes.h`); traits and concepts (`TIsSame`, `TNumericLimits`, `CDerivedFrom`...,
  `Core/Templates/TypeTraits.h`); `Algo`, `Memory`, `CString`, `Char`, `Maths`, `Random`, `Clock`,
  `DateTime`, `FileSystem`, `FileIO`, `Console` (`Core/`), `Process` (`Platform/`); `OpaaxString`,
  `OpaaxStringView`, `PathString`, `OpaaxStringID` (`OPAAX_ID("Name")`).
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
  `Physics/Box2D/`, miniaudio in `Audio/Miniaudio/`, OpenGL in `RHI/OpenGL/`, stb in the texture loader and
  the font baker, tinyfiledialogs in the editor's dialogs. Type ids come from
  `Core/Reflection/TypeInfo.h`. Gameplay code uses `Entity`, `World`, `Behaviour`: never the registry.
- **The standard library is a vendor too:** engine, editor, game and test code use the core types
  and functions above. std appears only in the files that wrap it (listed in
  `CMake/CheckStdUse.cmake`) and in the platform's OS code. A facility that has no wrapper yet gets
  one in Core first. Where a vendor takes or gives std types, the glue sits with the wrapper
  (`Core/String/OpaaxStringJson.h`: `ToJsonString`, `FromJsonString`, `ToJsonText`).
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

- **Component or behaviour:** the type with `OPAAX_PROPERTIES` (and the JSON macro for a component;
  `OPAAX_OBJECT(T, Opaax::Behaviour)` first in a behaviour's body), one `OPAAX_REGISTER_*` line, unit
  tests for its logic, a TestWorld probe or level when a game can see it. Old maps must still load:
  new fields have defaults (`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`).
- **Subsystem:** a `WorldSubsystem` or `GameInstanceSubsystem` subclass with `OPAAX_OBJECT` and one
  `OPAAX_REGISTER_OBJECT` line: its owner finds the class. Nothing in its constructor (the class
  default object is built with it); its services from `GetContext()`, valid from `Startup`; which
  worlds and when it ticks by overriding `ShouldCreate` and `GetTickOrder`. A unit test adds it to a
  world after `SetContext` (`GetSubsystems().Add<T>()`, then `StartupAll()`).
- **Game instance class:** a `GameInstance` subclass with `OPAAX_OBJECT`, `OnStart`/`OnEnd` overrides
  and one `OPAAX_REGISTER_OBJECT` line, named by the project's `gameInstanceClass`. Session features
  go in game instance subsystems instead, which every game makes.
- **Config:** a data struct with the JSON macro and `OPAAX_PROPERTIES` (a default on every field), a
  `TConfig<Data>` subclass with `OPAAX_OBJECT(Class, Opaax::Config)` and its `FileName()` (or
  `DECLARE_T_CONFIG`), and one `OPAAX_REGISTER_OBJECT` line. Read with `IConfigSystem::Get<T>()`; the
  Config panel draws it from its properties. See [Docs/Customizing.md](Docs/Customizing.md).
- **UI widget:** a `UIWidget` subclass with `OPAAX_OBJECT`, its saved fields in `OPAAX_PROPERTIES`, and
  one `OPAAX_REGISTER_OBJECT` line: canvases save it, the UI panel offers and draws it. Made with
  `NewObject<T>()` in code; its parent owns it.
- **Asset class:** an `Asset` subclass with `OPAAX_OBJECT` and `OPAAX_PROPERTIES` (a default on every
  field), one `OPAAX_REGISTER_OBJECT` line, unit tests for its file round trip and its hooks
  (`PostEditChange`). In the editor, an `AssetTypes()` entry for its label, category and glyph; an
  `IAssetEditor` only when a list of fields is not enough, its logic in pure functions beside it.
  Replacing an older per-type file format: a line in `AssetMigration::GetLegacyFormats`, then the
  content converted with `--upgrade-assets`. A class read from files of its own format (a `.png`)
  lists their extensions with `OPAAX_ASSET_SOURCE` and reads one in `LoadSource`. See
  [Docs/Objects.md](Docs/Objects.md).
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
- Windows: a build tree under `%TEMP%` misses header changes (MSBuild does not track files there,
  warning MSB8029), and stale objects then crash at run time. Keep checkouts and builds out of it.
- A `git worktree` has no submodules: run `git submodule update --init --recursive` in it before the
  first build (the configure step says so).
- `OPAAX_REGISTER_*` runs only in code linked whole: the engine and the game modules. The editor
  library is not, so a registration in an editor file is dropped by the linker; the editor registers
  through its `EditorExtensionRegistrar`, and makes its config by reading it.
- Core has namespaces named `Clock`, `Random`, `Algo`, `Memory`, `FileSystem`, `Console`, `Process`:
  a type of the same name, used under `using namespace Opaax`, is ambiguous. Name it otherwise.
- Visiting a `TAssetRef<T>` property needs `T::StaticClass()`, so `T` must be complete wherever the
  owner's properties are registered or drawn: an object class header (an asset, a widget) includes
  the asset classes it references, since every file that makes one builds its class; a component
  forward-declares them and its registration file includes them.
- An asset window records an undo step whenever no ImGui item is active: an editor that edits on a
  mouse drag keeps an item active for the whole drag (an `InvisibleButton` over the area), or every
  frame of it becomes a step.
