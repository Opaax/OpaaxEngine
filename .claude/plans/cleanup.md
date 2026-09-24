# Cleanup — dead files, dead config, dead accessors (2026-08-21)

> Not a milestone. A sweep the user asked for: *"There are a lot of stuff not used in the code, folders
> etc… opaaxproj are mentionned but I think code using it. Same for engine config a lot of configs params
> are not used! Asset manifest too."*

## Method

L10 is the standing rule: **grep is a seed, the build is the verdict.** Every deletion below was seeded by
a whole-tree grep (repo root, `Legacy/`+`Vendors/`+`OldProject/` excluded, **`Engine/Tests` INCLUDED** —
that omission has cost three sessions) and is only closed by three green presets.

## Decisions taken by the user

| Question | Answer |
|---|---|
| Dead config groups (Assets / Log / Physics / Render.Interpolation) | **Delete all four** |
| `OldProject/` | **Delete** |
| `Engine/Assets/` dead content (54 fonts, Test.bin, 2 textures) | **Delete all** |
| 14 zero-caller accessors | **Delete all** |
| `Engine/Source/Audio/` + `AudioManager` | **KEEP** — *"its just prebuild for future, is already using new subsystem"* |

**Not done, deliberately:** `IProjectManager`'s `startupScene`/`defaultScene` fallbacks. They look dead
(no `.opaaxproj` in the tree uses them once `OldProject/` goes) but three test cases pin them as X4
migration tolerance for project files *outside* this repo. Tolerance for input you cannot see is not
dead code.

## Steps

- **S1 — folders + tracked junk.** `OldProject/`, root `OpaaxTests/`, root `imgui.ini`;
  untrack (keep on disk) `.vs/` and `CMakeLists.txt.DotSettings.user`.
- **S2 — the asset manifest is dead end-to-end.** No C++ reads one; the reader
  (`AssetManifest`/`AssetRegistry`/`AssetScanner`) is entirely in `Legacy/Assets`. Delete both
  `AssetManifest.json`s **and** the CMake generate-if-missing + copy block that kept regenerating one.
- **S3 — `Engine/Assets/` content + its copy steps.** Fonts (8.5 MB copied to `bin/` every build),
  `Test.bin`, `Textures/`. The `Meshes` copy block goes too — that directory never existed.
- **S4 — the `Source/Assets/*` glob.** Six lines, two of them duplicated, for a directory moved to
  Legacy at M0.5.
- **S5 — `EngineConfigData`.** Delete `AssetSettings`, `LogSettings`, `PhysicsSettings`,
  `WorldBoundsSettings` and `Render.Interpolation`; regenerate `Sandbox/Configs/Engine.config`
  **byte-exact** (nlohmann `dump(4)`, keys sorted, no trailing newline — MP6 warns otherwise);
  update `EngineConfigDataTests`.
- **S6 — `Sandbox.opaaxproj`.** Strip `version`, `assetsRoot`, `assetsManifest`, `defaultScene`,
  `editorDefaultScenePath`. `ParseProjectIdentity` reads four keys; the file carried nine.
- **S7 — the creator templates, which is a DEFECT not clutter.**
  `OpaaxCreator/Templates/Configs/Engine.config` is in the pre-BO1b flat lowercase format and
  `Renderer.config` writes `clear_color` for a field named `ClearColor`. With `_WITH_DEFAULT` every
  value is **silently ignored** — a freshly created project boots at hardcoded defaults, no warning.
  Regenerate both from the live types.
- **S8 — 14 zero-caller members.** `World::Clear()` plus 13 one-line accessors.
- **S9 — quarantined tests.** `Tests/Assets/AssetIdResolveTests.cpp` and
  `Tests/Renderer/FontKerningTests.cpp` are commented out of CMake but still sit in *live* test dirs;
  the other four quarantined suites are already in `Tests/Legacy/`. Move them there.
- **S10 — gates.** Three presets green, `OpaaxTests` green, both hosts boot 0 err/warn.

## Review

### The one thing that went wrong: `World::Clear()` was NOT dead

It shipped in S8 and the build caught it — **5 callers, all in `Engine/Tests`**
(`WorldEntityTests` ×3, `MapSnapshotTests`, `ModuleRegistrarTests`). Restored, with
`Level::OnWorldCleared` and `WorldGuidRegistry::Clear`, which only looked dead because they
serve it.

**The cause is not "I forgot to grep tests" — I greppped them.** The command was

```
rg -n "\bClear\s*\(" … Engine/Source Editor/Source Sandbox Engine/Tests | rg -v "\.Clear\(\)|…"
```

The second `rg -v` was a noise filter for `m_Foo.Clear()` housekeeping calls. A *call site* is
spelled `lWorld.Clear()` — which matches `\.Clear\(\)` — so **the filter deleted precisely the
evidence the search existed to find**, and left only declarations, which read as "declared, never
called." [[L10]] says grep is a seed and the build is the verdict; that is the guard that worked.
[[L21]] names the deeper rule and it applies to a *grep* as much as to a test: the instrument must
not share a failure mode with the thing it measures. A dead-code search whose filter matches call
syntax cannot answer a dead-code question. → promoted as **L46**.

The other 13 accessors were re-checked with an **unfiltered** grep afterwards: zero remaining
references each, and the three presets agree.

### Gates

| Gate | Result |
|---|---|
| `debug-editor` | **OPAAX_BUILD_OK**, 0 errors |
| `debug` | **OPAAX_BUILD_OK**, 0 errors |
| `release` | **OPAAX_BUILD_OK**, 0 errors |
| `OpaaxTests` | **384 / 384 passed**, 6702 assertions, 7 skipped (baseline 383 / 6715 / 7 — +1 case for the unknown-group tolerance, −13 assertions on deleted fields) |
| `Sandbox.exe` boot | 0 err / 0 warn, world + sprite intact |
| `SandboxEditor.exe` boot | 0 err / 0 warn |

### What actually came out

- **-159 tracked files.** `OldProject/` (154), root `OpaaxTests/`, root `imgui.ini`, two
  `AssetManifest.json`, 54 fonts + 3 stray assets; `.vs/` (13) and the Rider settings untracked but
  left on disk.
- **8.5 MB stops being copied into `bin/` on every single build** — the fonts, plus the
  Textures/Meshes/manifest copy steps. `Engine/Assets/` is now `Shaders/` and nothing else.
- **`EngineConfigData` went from 5 groups to 2**, and every field left has a reader.
- **A real defect fixed on the way past:** the creator's `Engine.config` template was pre-BO1b flat
  lowercase and its `Renderer.config` said `clear_color` for a field called `ClearColor` — so since
  2026-08-20 every value in a new project's config was **silently ignored**. Nobody would have seen
  it until they wondered why a new game's window was titled "Opaax Engine".

### Left alone, deliberately

- **`Engine/Source/Audio/` + `AudioManager`** — user's call: *"its just prebuild for future, is
  already using new subsystem."* Still un-globbed, so still uncompiled.
- **`IProjectManager`'s `startupScene`/`defaultScene` fallbacks.** Zero users in the tree once
  `OldProject/` went, and three test cases pin them as X4 migration tolerance. Tolerance for input
  you cannot see is not dead code.
- **The `version` key in `.opaaxproj`** *was* dropped (BO1b's precedent verbatim: written, never
  read). The `.opaaxlevel`/`.opaaxmap` `version` keys stay — `LevelFile::Load` genuinely checks its.
- **Orphaned maps** (`MyMap`, `NewMap`, `Props` — in no level manifest) and the `SandboxPanel`
  deprecation: authored content, the user's call.

### Two things the sweep turned up that were not on the list

- **`OpaaxTests/` is not dead — it is REGENERATED.** `OpaaxTests.exe` resolves its project root to
  `<workspace>/OpaaxTests/` (`ResolveProjectLayout`'s `<workspace>/<exeStem>/` default) and rewrites
  `Configs/` + `Save/Log/` on every run. The tracked copy was stale *because* it was generated by an
  old build and committed by accident. Deleting it was right; the actual fix is the `.gitignore`
  entry, which is now there. It also served as a free end-to-end check: the file it regenerated after
  this change is **byte-identical** to the hand-written `Sandbox/Configs/Engine.config`.
- **`build/<preset>/bin/` accumulates and nothing prunes it.** `copy_directory` /
  `copy_if_different` never delete, so the release deploy still holds all 54 fonts, both
  `AssetManifest.json`, and `Sprites.opaaxmap` (deleted in `bcdb0b3`). Harmless in a dev build —
  nothing reads that tree — but it is precisely the stale decoy `Sandbox/CMakeLists.txt` warns
  about, and only a clean build dir clears it.

### Index state at hand-off — READ BEFORE COMMITTING ([[L9]])

Nothing is committed. **230 deletions + 2 renames are STAGED; every source edit is UNSTAGED.** A bare
`git commit` would therefore land the deletions *without* the code that compensates for them — a
committed state that does not build, [[L9]]'s corollary exactly. Stage the modified files too, or
commit by pathspec. `Sandbox/Assets/Maps/Main.opaaxmap` and `Sandbox/Assets/Levels/Main.opaaxlevel`
are the **user's own** pre-session work — keep them out of any cleanup commit.
