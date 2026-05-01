# Todo — OpaaxEngine Milestone Roadmap

> Source plan: `C:\Users\engue\.claude\plans\silly-petting-charm.md`
> Audit date: 2026-04-30 — verified against current code via three parallel Explore agents.
> Previous milestone (rotation + hierarchical parenting) archived at `tasks/done/2026-04-30_rotation-parenting.md`. Manual verification of section 7 there is **still pending** — build is clean (✓), six visual checks remain.

## Status legend
- [ ] pending
- [~] in progress
- [x] done

---

## Active milestone — M1 Asset Unification + Config Foundation

> Implementation pass complete (2026-04-30). Build clean against `debug-editor` preset. User verifies behaviour at runtime.

### Implementation
- [x] New `Engine/Source/Core/Config/EngineConfig.{h,cpp}` — JSON load/save via `nlohmann/json`, defaults match historical hardcoded constants, auto-generates a default file on first launch.
- [x] `CoreEngineApp` ctor: calls `EngineConfig::Load(OpaaxPath::Resolve("engine.config.json"))` after `OpaaxPath::Init()`; window now created from `EngineConfig::WindowTitle/Width/Height()`.
- [x] `CoreEngineApp::Initialize`: manifest paths now sourced from `EngineConfig::EngineManifestRelPath()` / `GameManifestRelPath()`.
- [x] `SpriteComponent::Serialize`: reverse-lookup via `AssetManifest::FindByPath(MakeRelative(absPath))`; writes logical ID when manifest knows the file, falls back to relative path otherwise.
- [x] `SpriteComponent::DeserializeImplementation`: stops pre-resolving; passes raw stored reference to `AssetRegistry::Load`, letting `ResolveToAbsPath` route the three cases.
- [x] Removed orphan `World m_World;` from `CoreEngineApp.h`. `GetWorld()` already routes through `SceneManager`.

### Verification (user-driven runtime)
- [x] Editor build clean (debug-editor preset). *(2026-04-30)*
- [ ] First launch generates `build/debug-editor/bin/Debug/engine.config.json` with the documented JSON shape.
- [ ] Edit `engine.config.json` → `window.width = 800`, `window.height = 600`, restart, confirm window opens at 800×600.
- [ ] Editor flow: open Asset Browser → click **Refresh** → drag a texture onto a sprite → save scene → exit → reopen → load. Saved JSON's `texture` field should be the logical ID (e.g. `Textures/Player`) once the manifest is populated.
- [ ] Backward compat: a scene file written before M1 (relative path in `texture`) still loads sprites correctly.
- [ ] No reference to `m_World` remains; `GetWorld()` works in editor.

### Out of scope (deferred)
- `AssetHandle` keyed on logical ID (M4 alongside RHI cleanup).
- Auto-running `AssetScanner` from engine init in non-editor builds (current pipeline already correct: editor scans → manifest committed → game ships manifest).
- `OpaaxLog::Init` consuming `EngineConfig::LogLevel()` (deferred to M8 polish).

---

## State Report (verified, condensed)

**Resolved since last audit**
- ✅ `CoreEngineApp::m_World` — orphaned dead member; world owned by `SceneManager` (`CoreEngineApp.cpp:258-263`).
- ✅ `OpenGLShader` uses Opaax `UnorderedMap` alias (`OpenGLShader.h:72`).
- ⚠️ `Camera2D` 1280×720 — now defaults only; `Startup` reads window size (`Camera2D.cpp:18-22`). Real fix is a config file (M1).
- 🟦 `OpaaxMath` separate library — DROP. Pure GLM aliases, zero custom code; revisit only if real custom math appears.

**Still open** — 13 items grouped into the milestones below.

---

## Milestone Roadmap

### M1 — Asset Unification + Config Foundation  *(NEXT — correctness + foundation)*

> Why first: removes the only known correctness bug (asset save/load divergence) and unblocks every future feature that needs configurable defaults.

- [ ] **Asset path unification.** Make `AssetRegistry` the single source of truth; `AssetManifest` becomes its persistence file (or merge them). Components write **manifest UUID**, loaders resolve via `AssetRegistry`. No more "find via manifest, load via registry" split.
  - Critical files: `AssetRegistry.h:25,138`, `AssetManifest.{h,cpp}`, `SpriteComponent.cpp:13,45`.
- [ ] **`Engine/Source/Core/Config/EngineConfig.{h,cpp}`** — JSON via existing `nlohmann/json`. Fields: `window.width`, `window.height`, `viewport.width`, `viewport.height`, `assets.engineRoot`, `assets.gameRoot`, `log.level`, `defaultScene`.
- [ ] Wire `EngineConfig` into `CoreEngineApp` startup; replace hardcoded constants (notably `Camera2D` defaults).
- [ ] **Delete orphan `CoreEngineApp::m_World`** (`CoreEngineApp.h:112`).
- [ ] Verification: load a scene with sprites, save, restart, load again — all texture references resolve with no warnings; window size driven by `engine.config.json`.

### M2 — Editor: Real Toolbar + PIE State + Save/Open

> Why second: rotation+parenting produced richer scenes; user needs a proper save/open UX and a real Play state machine before physics lands.

- [ ] `BeginMainMenuBar` with **File** (New / Open / Save / Save As / Recent / Exit), **Edit**, **View**, **Window**, **Help**.
- [ ] `EditorState` enum: `Editing | Playing | Paused`. Fold `PlayStopPanel` into the toolbar; viewport shows a tinted border per state.
- [ ] "Save Scene As" + "Open Scene" file dialogs (use `tinyfd` or a small ImGui modal — avoid native-only Win32).
- [ ] Critical files: `Editor/Panels/PlayStopPanel.{h,cpp}`, `EditorSubsystem.{h,cpp}:179-183`, new `Editor/Toolbar/MainMenuBar.{h,cpp}`, `Scene/SceneSerializer.cpp`.
- [ ] Verification: build editor, click Save As → pick path → reload editor → Open Scene → entities + parents + UUIDs intact.

### M3 — Sprite UX: Intrinsic Size + SpriteSheet + Animation

> Closes the "transform scale unpredictability" complaint and gives 2D content authors first-class atlas + animation workflows.

- [ ] Add `SpriteComponent::Size` (world units) decoupled from `Transform.Scale`. Final draw size = `Size × Transform.Scale` composed through hierarchy.
- [ ] New `SpriteSheetComponent` — atlas reference + grid (`cellW`, `cellH`, `columns`, `cellIndex`).
- [ ] New `SpriteAnimationComponent` — frame list, timing, loop. Drives `cellIndex` on the sibling `SpriteSheetComponent`.
- [ ] Inspector drawers + serialization for the two new components.
- [ ] Critical files: `ECS/Components/SpriteComponent.{h,cpp}`, new `SpriteSheetComponent`, new `SpriteAnimationComponent`, `Renderer/Renderer2D.cpp` (sprite size param), `Editor/Inspector/Drawers/`, `WorldRenderSystem.cpp`.
- [ ] Verification: a sheet-driven character animates in editor; nested-parent scaling produces predictable on-screen size.

### M4 — RHI Cleanup (prep for Vulkan)

- [ ] Move resource factories behind `IRenderAPI`: `CreateBuffer`, `CreateShader`, `CreateTexture2D`, `CreateVertexArray`.
- [ ] Wrap `Renderer2D::s_Data` (`Renderer2D.cpp:57`) in a `Renderer2DContext` instance owned by the renderer; mutex command submission.
- [ ] Backend stays GL-only; the seam is the deliverable.

### M5 — Physics 2D (Box2D)

- [ ] Vendor Box2D as a submodule; `PhysicsSubsystem` ticking on the existing `FixedUpdateAll` slot (`CoreEngineApp.cpp:177`).
- [ ] Components: `Rigidbody2DComponent` (Static/Kinematic/Dynamic), `BoxCollider2DComponent`, `CircleCollider2DComponent`.
- [ ] Two-way sync with `TransformComponent`; respect parent hierarchy (only root entities own bodies; children ride their parent transform).
- [ ] Inspector + serialization.

### M6 — Audio (miniaudio)

- [ ] Vendor miniaudio (single header).
- [ ] `AudioClip` asset type wired into the unified registry from M1.
- [ ] `AudioSourceComponent` (volume, pitch, loop, autoplay) + single global listener.
- [ ] Editor: clip preview button.

### M7 — Job System + Renderer Threading

- [ ] Lightweight `Core/Concurrency/JobSystem.{h,cpp}` — fixed worker pool, lock-free or single-mutex MPMC.
- [ ] Parallel ECS view iteration where component access is read-only.
- [ ] Renderer2D submission becomes thread-safe (prepped in M4).

### M8 — Polish & utilities

- [ ] `OpaaxString::Format(fmt::format_string, args...)` member API on top of fmt.
- [ ] Texture-based asset icons + thumbnail cache for the asset browser (replaces `"[ T ]"` chars at `Texture2DTypeActions.h:18`).
- [ ] Sweep remaining hardcoded constants into `EngineConfig`.

---

## Pre-flight before starting M1

- [ ] Close the seven verification checks in `tasks/done/2026-04-30_rotation-parenting.md` section 7.
- [ ] Update `CLAUDE.local.md` "CURRENT MILESTONE" to **M1 — Asset Unification + Config Foundation**.
- [ ] If `code/code-base.md` doesn't exist, create it with the project-structure summary captured during the audit (`Engine/Source/{Core,ECS,Editor,Assets,Renderer,Scene,RHI,Platform}` + `Game/{Source,Assets}`).
