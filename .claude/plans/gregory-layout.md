# Gregory layout — the runtime tree by layer

**OPEN 2026-09-11. Executed by the USER, by hand.** This file is the complete move list, the
include-rewrite map, the CMake edits and the gate for each step. It closes the item **X1** has
carried since 2026-07-19 (*"Next: Gregory-layer the live remainder"*).

Baseline at write time: `refresh_engine` @ `0fc7c4a`, **784 / 8824 / 7**. Nothing in this plan may
change that number — a reorg has no behaviour.

---

## The rule (three sentences — this is what becomes durable at close)

1. **One top-level folder per box in Gregory's runtime diagram (Fig 1.16)**, flat, in layer order:
   `Core` · `Platform` · `Application` · `Resources` · `RHI` · `Renderer` · `Physics` · `Audio` ·
   `Input` · `Animation` · `Engine` · `World`. A folder is a *layer or column*, never a lifecycle
   kind — "is an `EngineSubsystem`" is not a folder.
2. **`Resources/` is the MECHANISM only** (manager, handle, ref, pool, hold, view, format registry,
   load context, `BinaryResource`). **A resource TYPE lives with the domain that consumes it** —
   `TextureResource` under `Renderer/`, `InputAction` under `Input/`, `Prefab` under `World/`. This
   is the honest placement: four of the seven type families already depend *upward*
   (`TextureResource.h` → `RHI/Texture.h`, `FontFaceResource.cpp` → `Renderer/Text/FontBake.h`,
   `InputAction*` → `Engine/Input/*`, Prefab/Map/Level → `World/*`), and `World/Prefab/PrefabResource.hpp`
   already follows it.
3. **`World/` keeps components and world subsystems.** Gregory ch. 16 puts the component-based
   object model in Gameplay Foundations: a `ColliderComponent` is how an *entity* binds to physics,
   not part of the sim. So a domain is split as *sim* (`Physics/`) + *binding* (`World/Components`,
   `World/Systems`) — and `World/Components/` stays the one folder that IS the Inspector's
   "Add Component" list.

What stays deliberately: `RHI/` a peer of `Renderer/` (the boundary the GL backend sits behind);
`Engine/` as the tier root that owns engine subsystems, no longer hosting the domains' managers;
the **Editor tree untouched** (Gregory's layering is for the runtime; by-concern is Godot's editor
shape and fine).

---

## Target tree

```
Engine/Source/
├── Core/              Core Systems — unchanged, minus Window/
├── Platform/          Platform Independence — IPlatform, IFileSystem, Window, WindowEvents, Windows/
├── Application/       Host + app services (I4) — minus Services/Platforms/, minus WorldSpec.h
├── Resources/         Resources layer, MECHANISM ONLY
├── RHI/               unchanged
├── Renderer/          + RendererManager · Camera/ · Texture/ · Sprite/ · Text/ (+fonts)
├── Physics/           unchanged
├── Audio/             + AudioManager (still un-globbed — their "prebuild for future")
├── Input/             raw HID at root · Mapping/ (evaluator, modifiers, subsystem, the two assets)
├── Animation/         AnimationClip*, AnimationLibrary*
├── Engine/            Engine, EngineEvents, FrameInfo, EngineSubsystem.h, EngineEventBus, Config/, Modules/, Registries/, GameInstance/
└── World/             unchanged + WorldSpec.h; Systems/Movement/ gains the Mover/MoveMode assets
```

`Engine/Subsystems/` no longer exists when this is done.

---

## Mechanics that apply to EVERY step

- **Move with `git mv`** so history follows the file.
- **Rewrite includes from the REPO ROOT over tracked files only, `Legacy/` excluded** ([[L10]] —
  three strikes on an allow-list of source dirs; tests are consumers). Both delimiters, `"` and `<`.
  The file set, once:
  ```bash
  export PATH="/usr/bin:/bin:/c/Program Files/Git/cmd:/c/Windows/System32"
  FILES=$(git ls-files -- Engine/Source Editor/Source Sandbox Engine/Tests OpaaxCreator | grep -E '\.(h|hpp|cpp|inl)$' | grep -v '/Legacy/')
  ```
  and each rewrite is one `sed`:
  ```bash
  sed -i -E 's|#include ([<"])OLD/PREFIX/|#include \1NEW/PREFIX/|g' $FILES
  ```
  The rows in each step's table are **ordered most-specific first** — apply them in that order or
  a broad prefix eats a narrow one. The `Resources/` mechanism row is written so it cannot match a
  `Types/` path, so that step is order-independent.
- **Same-directory sibling includes need nothing.** Every file-relative include inside a moving
  folder (`#include "ResourceRef.hpp"`, `#include "WindowsFileSystem.h"`) names a sibling that
  moves with it — verified 2026-09-11, there are no `../` includes in any moving folder. No
  basename collides after any merge.
- **Nothing outside C++ names these paths** except `Engine/Tests/CMakeLists.txt` (step 8) and the
  engine glob (step 2). The `OpaaxCreator` templates include only `Application/*` and
  `Engine/Modules/*`, none of which move.
- **Gate, every step:** `OPAAX_NO_PAUSE=1 ./build.bat debug-editor </dev/null`, grep
  `OPAAX_BUILD_OK` ([[L8]] — the exit code lies). Then `build/debug-editor/bin/Debug/OpaaxTests.exe`
  → **784 / 8824 / 7**, unchanged. Then the leftover grep for that step's OLD prefixes must be empty.
  **Commit before starting the next step** ([[L17]]). `fast` is not enough — it skips the editor
  targets, and the editor is 620 of the rewritten includes.
- The glob is `CONFIGURE_DEPENDS`, so a moved file is picked up on the next build without a
  re-configure — **as long as its new top-level dir is in the glob list** (step 2, 5, 6).
- **A stale `.o` can mask a miss** ([[L14]]): the first build after each step recompiles every
  rewritten TU by construction, so a green here is a real green.

---

## Step 1 — Platform (14 files, 24 includers)

```bash
git mv Engine/Source/Core/Window/Window.h            Engine/Source/Platform/Window.h
git mv Engine/Source/Core/Window/Window.cpp          Engine/Source/Platform/Window.cpp
git mv Engine/Source/Core/Window/WindowEvents.h      Engine/Source/Platform/WindowEvents.h
git mv Engine/Source/Core/Window/WindowEvents.cpp    Engine/Source/Platform/WindowEvents.cpp
git mv Engine/Source/Application/Services/Platforms/IFileSystem.h    Engine/Source/Platform/IFileSystem.h
git mv Engine/Source/Application/Services/Platforms/IFileSystem.cpp  Engine/Source/Platform/IFileSystem.cpp
git mv Engine/Source/Application/Services/Platforms/IPlatform.h      Engine/Source/Platform/IPlatform.h
git mv Engine/Source/Application/Services/Platforms/IPlatform.cpp    Engine/Source/Platform/IPlatform.cpp
mkdir -p Engine/Source/Platform/Windows
git mv Engine/Source/Application/Services/Platforms/Windows/WindowsFileSystem.h    Engine/Source/Platform/Windows/WindowsFileSystem.h
git mv Engine/Source/Application/Services/Platforms/Windows/WindowsFileSystem.cpp  Engine/Source/Platform/Windows/WindowsFileSystem.cpp
git mv Engine/Source/Application/Services/Platforms/Windows/WindowsPlatform.h      Engine/Source/Platform/Windows/WindowsPlatform.h
git mv Engine/Source/Application/Services/Platforms/Windows/WindowsPlatform.cpp    Engine/Source/Platform/Windows/WindowsPlatform.cpp
git mv Engine/Source/Platform/WindowsWindow.h    Engine/Source/Platform/Windows/WindowsWindow.h
git mv Engine/Source/Platform/WindowsWindow.cpp  Engine/Source/Platform/Windows/WindowsWindow.cpp
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Application/Services/Platforms/Windows/` | `Platform/Windows/` | 4 |
| 2 | `Application/Services/Platforms/` | `Platform/` | 13 |
| 3 | `Core/Window/` | `Platform/` | 11 |

`Platform/WindowsWindow.h` has **zero** includers by path (the platform factory is defined in its
own `.cpp`); its `#include "WindowsWindow.h"` is same-dir and survives.

Leftover gate: `grep -rn 'Services/Platforms\|Core/Window/' $FILES` → empty. Then `Core/Window/`
and `Application/Services/Platforms/` directories are gone (`git mv` leaves no empties; check).

**Why here:** `I4` says Platform + Core are the app's layers — but Platform is ONE layer and had
three homes. `IPlatform`/`IFileSystem` are still *app services* (registered in `Bootstrap`); only
their folder changes. Nothing in `AppServiceLocator` cares where a header lives.

---

## Step 2 — Resources: the mechanism (19 files, 132 includers)

```bash
mkdir -p Engine/Source/Resources
for f in LoadContext.hpp ResourceConcept.hpp ResourceDependencyGraph.hpp ResourceFormat.cpp ResourceFormat.h \
         ResourceFormatRegistry.cpp ResourceFormatRegistry.h ResourceHandle.hpp ResourceHold.hpp \
         ResourceManager.cpp ResourceManager.h ResourcePath.h ResourcePathJson.h ResourcePool.hpp \
         ResourceRef.hpp ResourceTypeID.cpp ResourceTypeID.hpp ResourceView.hpp; do
  git mv Engine/Source/Engine/Subsystems/Resources/$f Engine/Source/Resources/$f
done
git mv Engine/Source/Engine/Subsystems/Resources/Types/BinaryResource.hpp Engine/Source/Resources/BinaryResource.hpp
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/Resources/Types/BinaryResource.hpp` | `Resources/BinaryResource.hpp` | 2 |
| 2 | `Engine/Subsystems/Resources/` **followed by a filename, no further `/`** | `Resources/` | 130 |

Row 2, exactly — it must NOT touch `…/Resources/Types/…`, which later steps own:
```bash
sed -i -E 's|#include ([<"])Engine/Subsystems/Resources/([A-Za-z0-9_]+\.(h|hpp))|#include \1Resources/\2|g' $FILES
```

**CMake** — `Engine/CMakeLists.txt`, the `ENGINE_CORE_SOURCES` glob. Add after the `Source/Engine/*`
trio:
```cmake
    "Source/Resources/*.cpp"
    "Source/Resources/*.h"
    "Source/Resources/*.hpp"
```
and replace the stale comment at the top of the block (it still describes "Phase B" and a `Core/`
that holds Log/Container/Time) with:
```cmake
    # One dir per Gregory runtime layer/column (Fig 1.16), flat, bottom-up. Resources/ is the
    # mechanism only; a resource TYPE lives with its consuming domain. Audio/ is deliberately
    # NOT globbed (prebuilt shell, registered nowhere). Rule + tree: .claude/plans/gregory-layout.md
```

`ResourceManager.h` includes `Engine/Subsystems/EngineSubsystem.h` — leave it, step 7 rewrites it.

Leftover gate: `grep -rnE 'Engine/Subsystems/Resources/[A-Za-z0-9_]+\.(h|hpp)' $FILES` → empty.
`Engine/Source/Engine/Subsystems/Resources/` now contains only `Types/`.

---

## Step 3 — Renderer takes its manager, its camera and its three asset families (26 files, ~51 includers)

```bash
git mv Engine/Source/Engine/Subsystems/Renderer/RendererManager.h    Engine/Source/Renderer/RendererManager.h
git mv Engine/Source/Engine/Subsystems/Renderer/RendererManager.cpp  Engine/Source/Renderer/RendererManager.cpp

mkdir -p Engine/Source/Renderer/Camera
git mv Engine/Source/Engine/Subsystems/Camera/CameraManager.h    Engine/Source/Renderer/Camera/CameraManager.h
git mv Engine/Source/Engine/Subsystems/Camera/CameraManager.cpp  Engine/Source/Renderer/Camera/CameraManager.cpp
git mv Engine/Source/Renderer/CameraView.h    Engine/Source/Renderer/Camera/CameraView.h
git mv Engine/Source/Renderer/CameraView.cpp  Engine/Source/Renderer/Camera/CameraView.cpp

mkdir -p Engine/Source/Renderer/Texture
git mv Engine/Source/Engine/Subsystems/Resources/Types/Texture/TextureResource.h    Engine/Source/Renderer/Texture/TextureResource.h
git mv Engine/Source/Engine/Subsystems/Resources/Types/Texture/TextureResource.cpp  Engine/Source/Renderer/Texture/TextureResource.cpp

mkdir -p Engine/Source/Renderer/Sprite
for f in SpriteSheetData.cpp SpriteSheetData.h SpriteSheetFile.cpp SpriteSheetFile.h SpriteSheetResource.h; do
  git mv Engine/Source/Engine/Subsystems/Resources/Types/SpriteSheet/$f Engine/Source/Renderer/Sprite/$f
done

for f in FontFaceResource.cpp FontFaceResource.h FontFamilyData.h FontFamilyFile.cpp FontFamilyFile.h FontFamilyResource.h FontStyle.h; do
  git mv Engine/Source/Engine/Subsystems/Resources/Types/Font/$f Engine/Source/Renderer/Text/$f
done
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/Resources/Types/Texture/` | `Renderer/Texture/` | 8 |
| 2 | `Engine/Subsystems/Resources/Types/SpriteSheet/` | `Renderer/Sprite/` | 14 |
| 3 | `Engine/Subsystems/Resources/Types/Font/` | `Renderer/Text/` | 15 |
| 4 | `Engine/Subsystems/Camera/` | `Renderer/Camera/` | 1 |
| 5 | `Engine/Subsystems/Renderer/` | `Renderer/` | 0 |
| 6 | `Renderer/CameraView` | `Renderer/Camera/CameraView` | 13 |

Row 6 is a file, not a dir — the pattern is `s|#include ([<"])Renderer/CameraView\.|#include \1Renderer/Camera/CameraView.|g`.

`CameraView` moves so `Renderer/Camera/` is a real folder (the resolve + the view), not one class.
`FontStyle.h` and the `FontFamily*` files join `Renderer/Text/` where `FontBake`/`FontFaceData`
already are — the family was split across two trees for one reason: the older idiom.

Leftover gate: `grep -rn 'Types/Texture\|Types/SpriteSheet\|Types/Font\|Subsystems/Camera\|Subsystems/Renderer\|"Renderer/CameraView' $FILES` → empty.

---

## Step 4 — Input: one column, raw at the root, mapping below it (25 files, 42 includers)

```bash
mkdir -p Engine/Source/Input/Mapping
for f in InputCodes.h InputEvents.cpp InputEvents.h InputKeyCodeList.h InputKeyNames.h InputManager.cpp InputManager.h InputTypesFwd.hpp; do
  git mv Engine/Source/Engine/Subsystems/Input/$f Engine/Source/Input/$f
done
for f in InputActionEvaluator.cpp InputActionEvaluator.h InputActionValue.h InputMappingSubsystem.cpp InputMappingSubsystem.h \
         InputModifiers.cpp InputModifiers.h InputTypes.h InputTypesJson.h; do
  git mv Engine/Source/Engine/Input/$f Engine/Source/Input/Mapping/$f
done
for f in InputActionData.h InputActionFile.cpp InputActionFile.h InputActionResource.h \
         InputMappingContextData.h InputMappingContextFile.cpp InputMappingContextFile.h InputMappingContextResource.h; do
  git mv Engine/Source/Engine/Subsystems/Resources/Types/Input/$f Engine/Source/Input/Mapping/$f
done
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/Resources/Types/Input/` | `Input/Mapping/` | 16 |
| 2 | `Engine/Subsystems/Input/` | `Input/` | 27 |
| 3 | `Engine/Input/` | `Input/Mapping/` | 15 |

Row 3 is safe only because rows 1–2 ran first: after row 2 no path starts with
`Engine/Subsystems/Input/`, and `Engine/Input/` matches nothing else in the tree.

**CMake** — add to the glob, after `Source/Resources/*`:
```cmake
    "Source/Input/*.cpp"
    "Source/Input/*.h"
    "Source/Input/*.hpp"
```

Why raw at root and mapping in a subfolder, not two peers: Gregory's HID column is one box —
"Physical Device I/O" beneath "Game-Specific Interface". The raw layer (`InputManager`, codes,
events) is the device; **§IM** is the interface built on it and the two assets belong to that
interface (`InputAction*` includes `InputTypes.h` + `InputKeyNames.h`, nothing else).

Leftover gate: `grep -rn 'Engine/Subsystems/Input\|Engine/Input/\|Types/Input' $FILES` → empty.
`Engine/Source/Engine/Input/` is gone.

---

## Step 5 — Animation (9 files, 19 includers)

```bash
mkdir -p Engine/Source/Animation
for f in AnimationClipData.cpp AnimationClipData.h AnimationClipFile.cpp AnimationClipFile.h AnimationClipResource.h \
         AnimationLibraryData.h AnimationLibraryFile.cpp AnimationLibraryFile.h AnimationLibraryResource.h; do
  git mv Engine/Source/Engine/Subsystems/Resources/Types/Animation/$f Engine/Source/Animation/$f
done
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/Resources/Types/Animation/` | `Animation/` | 19 |

**CMake** — add to the glob:
```cmake
    "Source/Animation/*.cpp"
    "Source/Animation/*.h"
    "Source/Animation/*.hpp"
```

`SpriteAnimationSubsystem` and `SpriteAnimatorComponent` stay in `World/` (rule 3). `Animation/` is
a column folder holding nine pure-data files today; it is where a tween/timeline/skeletal layer
lands, which is why it is top-level and not `World/Animation/`.

Leftover gate: `grep -rn 'Types/Animation' $FILES` → empty.

---

## Step 6 — Mover assets join the modes (8 files, 19 includers)

```bash
for f in MoveModeData.h MoveModeFile.cpp MoveModeFile.h MoveModeResource.h MoverData.h MoverFile.cpp MoverFile.h MoverResource.h; do
  git mv Engine/Source/Engine/Subsystems/Resources/Types/Mover/$f Engine/Source/World/Systems/Movement/$f
done
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/Resources/Types/Mover/` | `World/Systems/Movement/` | 19 |

No CMake change (`World/` is globbed). The Mover is Gregory's "Player Mechanics — Movement", which
his diagram puts in the *game-specific* tier; this engine ships it as a reusable foundation, and its
consumers (`FlyMoveMode`, `GroundMoveMode`, `MoverSubsystem`) are all under `World/Systems/`. Its
assets go where its modes are. `World/Systems/Movement/` is 15 files after this.

Leftover gate: `grep -rn 'Types/Mover' $FILES` → empty. **`Engine/Source/Engine/Subsystems/Resources/`
is now empty — `rmdir` it (and `Types/`).**

---

## Step 7 — Dissolve `Engine/Subsystems/`; WorldSpec goes home (6 files, 24 includers)

```bash
git mv Engine/Source/Engine/Subsystems/EngineSubsystem.h          Engine/Source/Engine/EngineSubsystem.h
git mv Engine/Source/Engine/Subsystems/EventBus/EngineEventBus.h    Engine/Source/Engine/EngineEventBus.h
git mv Engine/Source/Engine/Subsystems/EventBus/EngineEventBus.cpp  Engine/Source/Engine/EngineEventBus.cpp
git mv Engine/Source/Engine/Subsystems/Audio/AudioManager.h    Engine/Source/Audio/AudioManager.h
git mv Engine/Source/Engine/Subsystems/Audio/AudioManager.cpp  Engine/Source/Audio/AudioManager.cpp
git mv Engine/Source/Application/WorldSpec.h  Engine/Source/World/WorldSpec.h
```

| order | old include prefix | new include prefix | includers |
|---|---|---|---|
| 1 | `Engine/Subsystems/EventBus/` | `Engine/` | 11 |
| 2 | `Engine/Subsystems/EngineSubsystem.h` | `Engine/EngineSubsystem.h` | 9 |
| 3 | `Engine/Subsystems/Audio/` | `Audio/` | 0 |
| 4 | `Application/WorldSpec.h` | `World/WorldSpec.h` | 4 |

- **`AudioManager` leaves the build by moving.** `Audio/` is un-globbed on purpose (their *"prebuild
  for future"*); `AudioManager` is referenced by nothing live (one comment in
  `WorldSubsystemIdentityTests.cpp`, verified 2026-09-11). It is the manager of a shell that is
  registered nowhere — it belongs with the shell. When audio lands, `Audio/` is globbed and all four
  files come back together.
- **`WorldSpec`** is *which world to open* — a game concept. **I4** says the app layer stays
  ignorant of those; `World.h` already includes it, and `IEngine.h` is the one app service allowed
  to know engine concepts. `OpaaxApplication.h` keeps including it, now from `World/`.
- `Engine/Source/Engine/Subsystems/` is empty → `rmdir`. **The word "Subsystems" no longer names a
  folder in the runtime tree.**

Leftover gate — the whole plan's: `grep -rn 'Engine/Subsystems\|Application/WorldSpec' $FILES` → empty.
`ls Engine/Source` shows exactly the twelve dirs of the target tree plus `Legacy/`.

---

## Step 8 — Tests mirror the tree (8 files + one CMake list; optional, the suite does not care)

```bash
mkdir -p Engine/Tests/Core/Resources Engine/Tests/Core/Input
for f in ResourceSystemTests.cpp ResourceFormatRegistryTests.cpp TextureResourceTests.cpp SpriteSheetTests.cpp AnimationTests.cpp MoverAssetTests.cpp; do
  git mv Engine/Tests/Core/Engine/Subsystems/Resources/$f Engine/Tests/Core/Resources/$f
done
git mv Engine/Tests/Core/Engine/InputManagerTests.cpp  Engine/Tests/Core/Input/InputManagerTests.cpp
git mv Engine/Tests/Core/Engine/InputMappingTests.cpp  Engine/Tests/Core/Input/InputMappingTests.cpp
```

`Engine/Tests/CMakeLists.txt` lists sources **explicitly** — a moved test that is not re-listed is
silently never compiled, and the suite stays green with fewer cases. Edit lines 49–53, 56, 59, 67:
`Core/Engine/Subsystems/Resources/X.cpp` → `Core/Resources/X.cpp`; `Core/Engine/InputManagerTests.cpp`
→ `Core/Input/InputManagerTests.cpp`; same for `InputMappingTests.cpp`. **Then check the case count
is still 784** — that is the gate that catches a dropped line.

`Tests/Core/Engine/` keeps `CameraResolveTests`, `EngineEventsTests`, `GameInstanceTests`,
`ModuleRegistrarTests` — all engine-tier. Not touching `Tests/Renderer` vs `Tests/Core/Physics`
(the test tree's own inconsistency predates this and is not this job).

---

## Close — docs (do these in the LAST commit, same change as the tree per CLAUDE.md §0)

1. **ARCHITECTURE.md** — promote rule 1–3 above into a new **§LY — Layout** (three sentences + the
   target tree), placed right before **§PL**. Add to **PL** a step 0: *"Name its Gregory box → that
   is its folder (§LY). A resource type goes with its domain; a component or world subsystem goes
   in `World/`."*
2. **ARCHITECTURE.md** — sweep the nine live-path mentions (`grep -n 'Engine/Subsystems\|Core/Window\|Application/WorldSpec'`):
   rewrite the ones in live sections; leave historical ones in dated records as they are.
3. **X1** — already re-pointed at this file (2026-09-11).
4. **CLAUDE.local.md** — drop the *"Gregory-layer the remainder"* item if it is listed; record the
   close under RECENTLY CLOSED with the commit range and the unchanged **784 / 8824 / 7**.
5. **`Engine/CMakeLists.txt`** glob comment — done in step 2.

---

## Pitfalls, in the order they will bite

- **A red build in a file you did not touch is a rewrite miss, not a break** — the include path is
  the evidence; grep the OLD prefix over `$FILES` again. Every file-relative include was checked;
  the only way to a red is a table row skipped or applied out of order.
- **`build.bat fast` skips the editor targets** — 620 of the rewritten includes are in
  `Editor/Source`. A green `fast` after step 2 proves nothing about the editor ([[L24]]).
- **`build.bat test` builds only `OpaaxTests`** and writes to `build/debug-editor` ([[L70]]). Use the
  full `debug-editor` preset as the gate; run `OpaaxTests.exe` from its `bin/Debug`.
- **Step 8 can silently shrink the suite** — the explicit CMake list. The case count is the gate.
- **Commit per step, by pathspec** ([[L9]], [[L17]]) — a step's leftover-grep gate is a property of
  a clean tree. Sign them yourself; none of this is Claude's authorship.
- **A running `SandboxEditor.exe` locks the DLL** → `LNK1104` ([[L15]]). `tasklist` before building.
- The stale worktree `.claude/worktrees/magical-hermann-fef535/` still carries the OLD test paths.
  It is not in `git ls-files` of this tree and does not build; ignore it or delete it.

## Gate at the very end

`./build.bat debug-editor` → `OPAAX_BUILD_OK` · `OpaaxTests.exe` → **784 / 8824 / 7** · both hosts
launch once each (`Sandbox.exe`, `SandboxEditor.exe` — `ls -la` the exe against the DLL first,
[[L24]]) and the log shows the same `Sealed with N component type(s)` / `1 of 2 subsystem
candidate(s)` lines as before · `grep -rn 'Engine/Subsystems\|Engine/Input/\|Core/Window/\|Services/Platforms\|Application/WorldSpec\|Types/\(Texture\|Font\|SpriteSheet\|Animation\|Input\|Mover\)' $FILES` → **empty**.
