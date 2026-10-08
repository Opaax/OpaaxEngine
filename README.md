# OpaaxEngine

A 2D game engine in C++20 for Windows, Linux and macOS, with its editor. Games are written in C++
(components, behaviours, subsystems) and built from levels authored in the editor.

- **Rendering:** OpenGL 4.1 everywhere; batched sprites, sprite sheets and animation, text in many
  scripts, UI canvases; HDR with exposure and tonemapping, 2D lights (point, spot, global) with
  normal maps and emissive materials, soft shadows, ambient occlusion and bloom.
- **World:** an entity-component world (EnTT), prefabs, levels made of maps, behaviours with an update
  loop, events, timers, spawning and destruction.
- **Physics** (Box2D), **audio** (miniaudio), **input** with actions and mapping contexts.
- **Editor:** inspector, hierarchy, viewport with gizmos, play in editor, undo, asset editors, and
  export of a game into a folder that runs on another machine.
- **Automation:** a game or the editor can be driven by JSON requests (scripts in CI, AI agents):
  see [Docs/Automation.md](Docs/Automation.md).
- **Tests:** over a thousand unit tests, and TestWorld, a project whose levels exercise every
  feature and are checked by scripts.

## Build and run

You need CMake 3.24+ and a C++20 compiler: Visual Studio 2022 on Windows, GCC 13+ on Linux, Xcode's
Clang on macOS (what CI builds with). Clone with the submodules: `git clone --recursive`.

| Windows | Linux, macOS | What |
|---|---|---|
| `build.bat` | `./build.sh` | Debug build with the editor: SandboxEditor, TestWorldEditor |
| `build.bat run` | `./build.sh run` | The same, then starts the Sandbox editor |
| `build.bat release` | `./build.sh release` | Release games, no editor |
| `build.bat test` | `./build.sh test` | Builds and runs the tests |
| `build.bat export <Game> <Dir>` | `./build.sh export <Game> <Dir>` | Exports a game into a folder |

Executables land in `build/<preset>/bin/` (`bin/<Config>/` with Visual Studio). Linux needs the
X11/Wayland/GL development packages listed in `.github/workflows/build.yml`.

## Where to go next

- [Docs/QuickStart.md](Docs/QuickStart.md): a first game, from a new project to an exported build.
- [Docs/Customizing.md](Docs/Customizing.md): extending the engine and the editor.
- [Docs/Automation.md](Docs/Automation.md): driving a game or the editor from scripts.
- [Docs/AssetFormats.md](Docs/AssetFormats.md): the asset files (levels, maps, prefabs, UI, input,
  sprites, data), to write them by hand or from a tool.
- [Engine/Tests/README.md](Engine/Tests/README.md): writing tests.
- [AGENTS.md](AGENTS.md): working on the engine (conventions, rules, how to verify a change).

## Layout

```
Engine/          the engine: Source/<Module>/, Assets/ (shaders, fonts, textures), Tests/, Vendors/
Editor/          the editor library: panels, commands, documents, automation, export
Sandbox/         a game for trying things by hand
TestWorld/       a game whose levels test every feature (TestWorld/Tests/*.json)
OpaaxCreator/    the project generator (MakeOpaax.bat / MakeOpaax.sh) and its templates
CMake/           build helpers: opaax_add_game(<Name>) declares a game project
Docs/            documentation
Archive/         older code and documents, kept for reference
```
