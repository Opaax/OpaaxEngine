# How the engine is built

The parts of the engine, how a frame runs, and the main design decisions with what they give and
what they cost. Read it before a change that crosses modules; [Customizing.md](Customizing.md) shows
how to extend each part.

## The parts

| Module | Holds |
|---|---|
| `Core` | Types and aliases over the standard library, strings and paths, files, time, logging, reflection (`OPAAX_PROPERTIES`), events, jobs, maths |
| `Platform` | The window and input devices (GLFW), the file system |
| `RHI` | The rendering interface and its OpenGL 4.1 backend (`RHI/OpenGL`) |
| `Renderer` | Sprites, text and debug drawing in batches, cameras, and the HDR path: lights, soft shadows, ambient occlusion, bloom, tonemapping (`Post/ScenePipeline2D`) |
| `Object` | The root of the engine's object types: classes with properties and a default object, the table that handles and object pointers resolve through ([Objects.md](Objects.md)) |
| `Resources` | The resource manager: one pool per type, shared, reference counted, loaded on demand or in the background; assets (`Resources/Asset`): one `.opaaxasset` format for every asset class, references between assets, the conversion of old formats |
| `World` | Worlds (EnTT), entities, components, behaviours, levels and maps, prefabs, their files |
| `Physics` | Colliders, rigidbodies, contacts and queries over Box2D (`Physics/Box2D`) |
| `Audio` | Sound: the engine's own mixer (voices, buses, fades, spatial sound) on an audio thread, clip definitions and banks, sources and the listener; the device and the decoders behind a backend (`Audio/Miniaudio`) |
| `Save` | What a game keeps for a player: `SaveGame` classes saved by their properties, one file per slot in the player's own folder, written whole or not at all; each game's `SaveGameSubsystem` saves and loads them now or on a worker ([Saving.md](Saving.md)) |
| `Online` | The player's store (Steam): the player, names, leaderboards, the overlay, one set of calls over a backend per store, a fake store for tests, none when offline; each game's `OnlineSubsystem` ([Online.md](Online.md)) |
| `Input`, `Animation`, `Movement`, `UI` | Actions and mapping contexts, sprite animation, character movers, UI canvases |
| `Engine`, `Application` | The application, its services, the registries, the frame loop and the configs (`Engine/Config`: one `Config` object class per `.config` file, with the player's settings over it: [Settings.md](Settings.md)) |
| `Commands` | The app's command registry (`ICommandRegistry`, always there): each command's spec (parameters, cheat, dev only), scoped handles, and the typed line read into a request (`CommandText`) |
| `Automation` | The request runner, its two transports (a script, an inbox folder) and the engine's commands |
| `DevConsole` | The in-game console of dev builds: the line, history, completion and output as pure parts, and the game instance subsystem that draws it and holds the game's input ([Console.md](Console.md)) |

The editor is a separate library (`Editor/`) on top of the engine: panels, commands, documents
(the files being edited), a window per open asset, undo, Play In Editor, its own automation commands
and the game export. A game is a module of its own types, linked into its runtime and into its editor
(`opaax_add_game(<Name>)`).

## A frame

1. Finished background jobs report, the frame's queued events are delivered, and a level asked for
   during the last frame replaces the current one.
2. The frame clock measures the frame, capped at 0.25 s so a stall never becomes a jump. With vsync,
   it counts the frame as whole refreshes of the screen: a timer reads a steady screen with noise, and
   motion drawn at the noisy time shakes. What a frame of whole refreshes leaves carries to the next,
   so the game's time keeps to the screen's; the screen's real rate is measured, not taken from its
   stated one (60 Hz for 59.94). A frame that is no whole number of refreshes (a stall) counts as
   measured, and a few in a row (frames far shorter than a refresh: vsync not holding) stop the
   snapping until the frames keep to the refresh again. `Time.bSnapToRefresh` turns it off, for
   variable-refresh screens.
3. **Update:** the engine's own subsystems update (the resource manager collects what is no longer
   used), and the active world's subsystems run once, in their order: input (100), gameplay (200,
   the behaviours' `OnUpdate`), physics (300), after physics (400, the movers), the default (500),
   presentation (600: animation, audio), then debug drawing (700).
4. **Fixed steps:** as many steps as the time owed (`Time.FixedRate` in `Engine.config`, 60 a second by
   default; a test plays at a screen rate of its choice with `--frame-pacing`), at most
   `Time.MaxStepsPerFrame` (8): a frame owing more drops the rest and the game slows for it, instead
   of slow steps making every frame longer (the spiral). Each step runs the
   subsystems' fixed update in the same order, so a behaviour's `OnFixedUpdate` comes right before the
   physics step.
   Around each step the world records what a step changes for the entities that ask for it: the
   cameras' poses and `OrthoSize` (`CameraInterpolation`: every camera, so one that takes the view over
   is framed between its steps from its first frame), and the pose of any entity gameplay
   moves in a step (`TransformInterpolation::RecordSteps`). At the start of a step, before those
   move on, each entity drawn between steps notes whether the last step moved it and whether it
   moved since; a camera whose view one entity moves both ways, step after step, is reported as
   mixed (its `rhythm`) with a warning: it would jitter on screens faster than the steps.
5. **Render:** the active camera frames the world, then each view is drawn. Moving bodies and the
   camera (its pose and its zoom) are placed between their last two steps, so motion is smooth at
   any frame rate and a camera following a body keeps it still on screen. A view that shows another
   camera (the editor's Camera Preview) names it, and the renderer frames it as it draws, at the
   same alpha as the poses (`SubmitCameraView`): it matches the game's view of that camera exactly.
   What the editor draws on entities (the selection outline, icons, light gizmos) is queued just
   before the frame is drawn (`IEngine::AddBeforeDraw`), at that alpha, so it sits on the sprites.
6. **Present:** under a frame cap (`Window.MaxFrameRate`, while a game runs) the frame first waits for
   its turn (`FrameLimiter`); with vsync, a cap that divides the screen's rate is the swap interval
   instead, each frame shown for the same whole refreshes, as Unity's `vSyncCount` and Unreal's
   `rhi.SyncInterval` do. Then the back buffer is shown, and vsync waits for the refresh.

A paused world (in the editor) skips its part of 3 and 4; a step runs it for one frame. It is drawn
as it was on its last frame (the same alpha), so it holds still, and once resumed it goes on from
that picture: the time spent paused is not simulated.

Sound is mixed on its own thread, a few milliseconds ahead of the audio device: during the frame the
game only sends it commands (play, stop, move, fade), and the audio update frees the sounds that
ended.

## Decisions

### Types register themselves

A type is added with one line beside its definition (`OPAAX_REGISTER_COMPONENT(Health)`); the engine
runs the registrations when it starts, kind by kind (object classes, components, behaviours...). Object classes include the game and world subsystems: an owner finds them in the
class registry, so no subsystem list exists either.

- **For:** no central list or registrar function to edit, for the engine and a game alike. A type
  registered from several files is registered once.
- **Against:** it relies on static initialisation and on linking the libraries whole
  (`$<LINK_LIBRARY:WHOLE_ARCHIVE>` in `opaax_add_game` and the engine's target): a library linked
  another way loses its types without an error. A component is saved under its C++ name unless it
  uses the `NAMED` variant, so renaming the type renames it in the files.

### An entity-component world behind handles

Each `World` owns an EnTT registry. Gameplay code reaches it through `Entity`, `World` and
`Behaviour`, never the registry.

- **For:** compact storage and fast iteration where it matters (rendering, physics sync); components
  are plain structs, saved and inspected by reflection; the vendor stays replaceable and tests build
  worlds without a window.
- **Against:** every EnTT feature a game needs goes through a wrapper first.

### Entities named by guid, names kept unique

Every entity has a guid, which never changes and is its identity in files. A field that names another
entity is an `EntityRef` (its guid), as Unity and Unreal reference objects rather than names:
`Behaviour::Resolve` finds it. A copy, a duplicate or a prefab placement re-points the references
among the entities it makes at its own (`EntityClone`, `PrefabFactory::BuildInstance`), so a turret
copied with its target aims at the copy. Names stay for people and for lookups of entities made at
run time (`Behaviour::FindEntity`, a camera's `Target`): the editor names everything it makes or
renames uniquely in its document (`UniqueName`), entities and UI widgets alike, and warns when a file
opens with names in common.

- **For:** a reference survives renames and copies; a name lookup finds one thing.
- **Against:** a reference names an entity of its own world only; a runtime-spawned entity has no
  guid a file could know, so it is still found by name.

### Behaviours are C++ classes

Gameplay code is a `Behaviour` subclass with hooks (`OnStart`, `OnUpdate`, `OnFixedUpdate`,
`OnDestroy`), events, timers, spawning and physics calls, stored like a component.

- **For:** the speed, the debugger and the whole engine API, with no binding layer to keep in step;
  the lifecycle has firm rules (started before any update or event, `OnDestroy` exactly once, with
  the entity whole). Fields are reflected, so they are saved and edited like a component's.
- **Against:** no hot reload: changed gameplay code means a build and an editor restart. Each
  behaviour is a virtual call per frame: right for thousands of entities; logic over many more
  belongs in a world subsystem that iterates components.

### Levels made of maps

A level lists maps; one is persistent, the others are mounted on top. Map files are JSON with
entities sorted by id and line endings fixed, so they diff and merge well.

- **For:** several people (or agents) can work on different maps of one level, and a map can be
  listed by several levels; files written by hand or by a tool are accepted, then rewritten in the
  engine's form by Resave Level.
- **Against:** more files than a single scene, and every entity belongs to one map, chosen when it
  is created (the focused one in the editor).

### Shared, reference-counted resources

Resources are loaded once and shared by everything that uses them (an edit world and its Play
clone, several worlds at once). One no longer used is released during the next frame's update, so
a pointer taken this frame stays valid until then.

- **For:** Play In Editor loads nothing again; loads can run in the background; a resource can be
  reloaded in place, and every holder sees the new data.
- **Against:** a cached copy can outlive its file, so whatever writes a file reloads it (the editor
  does it for every document it saves). Resources are keyed by their path string, so one file must
  always be named in the engine's form (`IPaths`).

### Assets are objects of one root class

Every asset type, the engine's and a game's, is a class derived from `Asset`, itself an `Object`: it
names its parent and lists its fields (`OPAAX_OBJECT`, `OPAAX_PROPERTIES`), registers with one line,
and gets the rest from the base: one `.opaaxasset` format (its class and its properties), loading and
sharing through the resource manager, references between assets (`TAssetRef`), a window of its own in
the editor drawn from its properties (or its own editor), automation (`asset.*`). Objects live in one
table by slot and generation, so a handle to a destroyed object reads null, and the three pointer
kinds (owning, reflected, weak) are those a garbage collector needs. Unreal's `UObject` and
`UDataAsset`, Godot's `Resource`. Subsystems are object classes as well: a world or a game makes one of
each registered class, asking its default object whether to and where it ticks (Unreal's subsystem
collections). Components stay plain structs; levels, maps, prefabs and canvases keep formats of their
own.

- **For:** a new asset type is a class and a line, with no file format, loader, document, panel, undo
  steps or save command to write (the sheet, clip, library, font family, input and movement types each
  had their own); the editor, automation and the conversion of old files work the same for every
  class; a garbage collector can come later without changing the pointers' API.
- **Against:** fields must be types the reflection knows (anything richer takes a custom editor or a
  struct with `OPAAX_PROPERTIES`); a file names its class, so a renamed class needs an alias line
  (`OPAAX_REGISTER_OBJECT_ALIAS`); objects are not copyable; an undo step stores the whole property
  set, fine for assets, not for large data.

### OpenGL 4.1 everywhere, behind an RHI

One backend runs on Windows, Linux and macOS. Shaders are written once (GLSL 4.50 with explicit
bindings) and ported to 4.10 when loaded.

- **For:** the same picture on every platform, and on Mesa's software renderer, which CI renders
  with; the HDR path brings what the renderer lacked: 2D lights with normal maps and emissive
  materials, soft shadows, ambient occlusion, bloom, exposure and tonemapping.
- **Against:** no compute shaders, 16 samplers per stage, fixed budgets (32 lights and 16 shadowed
  lights per view); macOS stops at 4.1 and has deprecated OpenGL, so a Metal or Vulkan backend
  would be the next step, behind the same RHI.

### An editor of commands

The editor is ImGui panels over an `EditorContext`. Actions are commands dispatched by tag, and
operations over the context (`EntityOps`, `MapOps`...): menus, shortcuts, panels, game editor modules
and automation call the same ones, and edits go through the undo stack. Play In Editor clones the
edit world and throws the clone away on Stop.

Copy and paste go through the system clipboard as readable JSON in an envelope that names what it
holds (`EditorClipboard`), so a paste refuses another kind and a copy survives between editors. The
focused panel declares what Ctrl+C, Ctrl+X, Ctrl+V and Ctrl+D run (`PanelDesc`), as it does for Save,
Undo and Delete: a copy acts on the document in front (entities in the Hierarchy and the Viewport,
the prefab's in the Prefab panel, widgets in the UI panel), and a panel that declares none keeps the
keys. Entities copy through `EntityClone`, which gives the copies
new guids and keeps a whole prefab placement a placement.

- **For:** an agent's edit is a user's edit (undoable, same checks); Stop needs no undo; a game adds
  panels, commands and inspectors through `EditorExtensionRegistrar` without touching the editor.
- **Against:** an immediate-mode UI redraws every frame and has little layout or styling of its own.
  File pickers and questions are the OS's own dialogs, which block: automation keeps them closed and
  refuses what would need them, and the commands an agent uses take paths instead.

### Automation by JSON requests

A game or the editor runs requests from a script (`--exec`) or from files dropped in a folder
(`--automation`), and answers each one. Checks (`expect.*`) make a script a test.

- **For:** any tool that writes files can drive it, with no socket or library; the same commands
  work in a game and in the editor; the CI feature tests are plain scripts.
- **Against:** the inbox is polled once a frame, and the app only answers: it does not push events.

### One command registry for scripts and the console

Every command lives in one registry, an app service there in every run. A script's runner and the
console each keep their own queue over it, so a line typed while a script waits runs at once. A
command declares its parameters (the console checks a typed line, Tab completes it), whether it is a
cheat (running it publishes `CheatUsed` and marks the run) and whether it is dev only (a ship build
leaves it out, and the console with it). Game code registers for a lifetime and keeps the handle.

- **For:** one list for agents, tests and people; a command added by a game is typed and scripted
  alike; a run that used a cheat is known whatever ran it.
- **Against:** scripts are not checked against the declared parameters (their JSON goes straight to
  the handler), and the engine's older test cheats stay in ship builds so the release feature tests
  can run: a shipped game must check `HaveCheatsRun` before posting a run.

### Saves in the player's folder, written whole

A save is an object of a `SaveGame` class, written as text by its properties, one file per slot.
Two layers: `ISaveStorage`, an app service, holds the files (in the player's own folder: the OS's
user data folder in a ship build, the project's `Save/` in a dev build, as Unreal keeps its
`Saved/SaveGames`); the game's `SaveGameSubsystem` turns objects into those files, and lives and
ends with the game. A write goes to a temporary file, flushed, then renamed over the slot. The
background requests run on one worker job at a time, in the order asked, and answer at the
subsystem's next `Update`.

- **For:** a ship build never writes where it was installed; a crash or a power cut keeps the last
  save whole; the automation and the editor read the slots with no game running, and a platform's
  own save API can replace the files; a load asked after a save reads it; stopping Play In Editor
  writes what was asked and never calls back into a game that is gone; a person can read a save.
- **Against:** a save is turned into text on the main thread when it is asked, so a very large one
  costs that frame; the requests share one worker, so a long write delays the loads behind it;
  text is larger than a binary format and nothing checks that a save was not edited by hand.

### The player's settings over the project's configs

A config holds two layers: the project's values (its file, what the Config panel edits) and the
player's over them, shaped like the file, an item of a named list (the audio buses) by its name. The
engine reads one set, the project's with the player's over them. `IUserSettings`, an app service, keeps
the player's values in the `Settings` slot of the player's saves and puts them over the configs: in a
game from the start, before the window opens; in the editor only while Play runs. As Unreal keeps
`GameUserSettings.ini` in the player's `Saved/` folder over the project's defaults, and Godot a
`ConfigFile` in `user://`. The player's keys are a section of the same file that the input mapping
keeps: a mapping is named by its context asset, its action and the key the asset gives it, so a key
rebound before its context is added waits for it, and a project change drops only the keys it leaves
without a mapping.

- **For:** a settings menu changes what the engine reads at once, through the change events each
  config already has; the project's files never take a player's values; a game's own options get the
  same two layers by being a config; a project change (a bus taken out) drops only the player's values
  it leaves without a place; one file a person can read, kept with the saves.
- **Against:** a change is checked by reading the whole config again (fine at a menu's pace, not every
  frame); values go through JSON text between the menu and the config; the values for a config no app
  registers stay in the file unchecked.

### One online interface, a backend per store, loaded at run time

A game asks `IOnlineServices` (an app service) or its `OnlineSubsystem`, never a store. Each store is
a backend that gives the feature interfaces it has (identity, leaderboards, files, achievements, the
overlay) and null for the others; `OnlineCapabilities` says up front what is there, and a missing
feature answers `Unsupported`, as Unreal's online subsystems and EOS's interfaces split a store into
features. The services answer every request at the next `Pump` in the engine loop, never inside the
call. A store's SDK is compiled only where a developer has it, and its library is opened while the
game runs, never linked; a store that does not start leaves the null backend, which answers
`Unavailable`.

- **For:** a game is written once for Steam, Epic or GOG, and against their limits with the fake
  store's profiles; a game starts with no store, no SDK, no library, offline, and is DRM-free; the
  services keep what every store needs once (board handles, names, the friends range from a friends
  list, a file attached only to a kept score); answers never come inside a call, whatever the store
  does; feature tests and CI run on the fake store.
- **Against:** CI never compiles a store's backend (no SDK there), so a change to it is checked on a
  developer's machine with the store running; one store per run; what a store has beyond the shared
  interfaces needs a new interface first.

### The engine mixes; an audio library only plays

The mix is engine code (`Audio/Mixer`): voices, the bus tree, fades, resampling, distance and pan. A
backend only opens the device and decodes files (`IAudioDevice`, `IAudioDecoder`), like the RHI hides
OpenGL. The game thread sends commands through a lock-free queue to an audio thread that mixes into
a ring buffer the device reads; handles are reserved on the game thread, so a sound is playing from
the call that starts it. What plays is described by data: clip definitions (variations, chosen on the
game thread with a seeded random) and banks that load sounds before they are needed.

- **For:** every backend sounds the same, and switching library is a new folder and a case in
  `AudioBackendFactory.cpp`; the mix runs without a device, so tests render it into a buffer and
  measure it; no lock between the game and the audio; a full voice pool replaces the least important
  sound; no sound file is read or decoded on the game thread when it plays.
- **Against:** the mixer is ours to keep (linear resampling, no effects, reverb or HRTF); mixing ahead
  adds about 20 ms of latency over mixing in the device's callback; a sound in no bank plays a frame or
  two late, once loaded; OGG needs a decoder the backend does not have yet.

### Vendors only behind their wrappers

EnTT, GLFW, Box2D, miniaudio, OpenGL, stb and the store SDKs (Steamworks) are each used only inside
the code that wraps them, and the folders follow the modules above.

- **For:** replacing a library touches only the code that wraps it; the rest reads as the engine's
  own API, which ended the old code being spread everywhere (what is left of it is in `Archive/`).
- **Against:** a wrapper to extend before a game can use a new vendor feature.

### The standard library behind the core types

The standard library is treated as one more vendor. Code uses the engine's names: containers and
pointers through aliases (`TDynArray`, `TUnorderedMap`, `TUniquePtr`... in `Core/OpaaxTypes.h`),
traits and concepts (`Core/Templates/TypeTraits.h`), `Algo`, `Memory`, `CString`, `Maths` and
`Random`, `Clock` and `DateTime`, `FileSystem` and `FileIO`, `OpaaxString`, `OpaaxStringView` and
`PathString`, `Process`. Only those files, the platform's OS code and two test suites of the boundary
write `std::`; `CMake/CheckStdUse.cmake` (CTest `StdUse`) lists any other use. Most aliases are the
standard types for now: what the engine owns is the name and the one place it is defined, as engines
commonly do with their own containers and strings (Unreal's `TArray` and `FString`, the custom
allocators of Gregory's *Game Engine Architecture*).

- **For:** one place decides how a facility behaves on every platform (UTF-8 file names on Windows,
  `Maths` giving the standard results, a command's exit code on Linux and macOS); a type can get its
  own implementation or allocator later without touching its users; the code reads one way, in the
  engine's naming, and the check keeps it so.
- **Against:** a facility with no wrapper needs one before its first use; an alias is the standard
  type, so vendor code and the debugger still show `std::vector`; Core's namespaces (`Clock`,
  `FileSystem`...) take names a game might want for its own types.

### Tests at three levels

Unit tests (doctest, `Engine/Tests`) for logic kept free of the GPU and the window; TestWorld, a
game whose levels exercise every feature, checked by scripts; and CI building every preset on
Windows, Linux and macOS, with the feature tests on Linux.

- **For:** a change is checked the way a game uses it, the editor included; every platform builds
  every preset and runs the unit tests.
- **Against:** feature tests need a display (a virtual one on CI) and run in game time, so a check
  after a timed event needs margin on a slow machine.

### Games beside the engine, or in repositories of their own

The engine's CMake works as the top-level project, its games beside it, or as a subdirectory of a
game's own repository ([GameRepository.md](GameRepository.md)). A dev build bakes two folders: the
workspace, the top-level project that holds the games, and the engine's own; a ship build finds both
next to the executable. The engine's games and unit tests are built only when it is the top-level
project.

- **For:** a game keeps its history, issues and CI apart and pins the engine at a commit, as a
  submodule; nothing is installed or registered, unlike Unreal's engine association or O3DE's
  registered engines; MakeOpaax makes either kind, and the engine's CI builds a game repository.
- **Against:** each game repository builds the engine from source; a setting that games need goes on
  their targets (`opaax_configure_target`), since folder-wide CMake settings stop at the engine's
  folders.
