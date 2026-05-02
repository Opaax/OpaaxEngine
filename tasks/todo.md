# Todo — OpaaxEngine Milestone Roadmap

> Source plan: `C:\Users\engue\.claude\plans\silly-petting-charm.md`
> Audit date: 2026-04-30 — verified against current code via three parallel Explore agents.
> Archives: `tasks/done/2026-04-30_rotation-parenting.md`, `tasks/done/2026-04-30_m1-asset-config.md`, `tasks/done/2026-05-01_m2-step1-step2.md` (all verified & pushed).

## Status legend
- [ ] pending
- [~] in progress
- [x] done

---

## Active milestone — M2 Editor: Real Toolbar + PIE State + Save/Open

> **Why now:** the rotation+parenting + M1 work produced richer, persistable scenes. The editor still has only an ad-hoc Play/Stop bar — users need a real File menu, a proper Editing/Playing/Paused state machine, and Save/Open dialogs before physics (M5) lands.

### Pre-flight (do first when reopening — Step 3 next)
- [x] Re-read `tasks/done/2026-04-30_m1-asset-config.md` for the current asset-ID conventions.
- [x] Skim `Engine/Source/Editor/EditorSubsystem.{h,cpp}` and the new `Engine/Source/Editor/Toolbar/MainMenuBar.{h,cpp}` — Step 3 wires the existing stubbed File-menu callbacks (`MainMenuBar.cpp` Draw* methods) to real dialog calls.
- [x] Steps 1 + 2 archived in `tasks/done/2026-05-01_m2-step1-step2.md`.
- [ ] **Step 3 entry point**: drop straight into a plan-mode pass for Save/Open dialogs. Confirm `tinyfiledialogs` vs alternatives (current recommendation: vendor `tinyfiledialogs` — single header, native dialogs, ~5 KB).

### 1. Main menu bar
- [x] New `Engine/Source/Editor/Toolbar/MainMenuBar.{h,cpp}` — encapsulates `ImGui::BeginMainMenuBar` / `BeginMenu` blocks. Drawn first per frame from `EditorSubsystem::DrawPanels`.
- [x] **File**: New Scene · Open Scene… · Save Scene (Ctrl+S) · Save Scene As… · separator · Recent (placeholder, no persistence in M2) · separator · Exit. (New/Open/Save/SaveAs stubbed — log "TBD M2 Step 3"; Exit calls new `CoreEngineApp::RequestQuit()`. Ctrl+S text shown but binding lands in Step 3.)
- [x] **Edit**: Undo / Redo (greyed out — no command system yet; visible to advertise the surface).
- [x] **View**: per-panel toggles (Hierarchy, Inspector, Asset Browser, Viewport, Play/Stop). State stored on `EditorSubsystem` as 5 booleans + ref getters; `DrawPanels()` gates each panel.
- [x] **Window**: Reset Layout (stretch — currently logs TBD; `LoadIniSettingsFromMemory` not wired).
- [x] **Help**: About modal (hardcoded `0.1.0-dev` for now — `EngineConfig` exposes no version yet; TODO M8 polish to wire CMake-generated `BuildVersion.h` with commit hash).

### 2. PIE state machine
- [x] New `Engine/Source/Editor/EditorState.h` — `enum class EEditorState { Editing, Playing, Paused };` + `EditorStateToString()`.
- [x] `EditorSubsystem` owns the current state + `GetEditorState()` + transitions (`EnterPlayMode` / `PauseToggle` / `ExitPlayMode`); `SetEditorState()` logs every transition.
- [x] Replace `PlayStopPanel` content: Play / Pause / Stop buttons moved into `MainMenuBar` (right-aligned via `SetCursorPosX`). `PlayStopPanel.{h,cpp}` deleted; member, show-flag, View-menu entry, and `IsPlaying()` getter removed.
- [x] Transitions: `Editing → Playing` (Play), `Playing ↔ Paused` (Pause toggle), `Playing|Paused → Editing` (Stop). Snapshot taken once on Enter; Pause does NOT re-serialize.
- [x] Gate the world's gameplay update — chose **(a) boolean on subsystem**: `IEngineSubsystem::IsPlayOnly()` virtual default false; `EngineSubsystemMgr::UpdateAll(dt, bAllowPlayOnly)` and `FixedUpdateAll(dt, bAllowPlayOnly)` skip play-only when not allowed. `CoreEngineApp::Run` computes `bAllowPlayOnly` from `EditorSubsystem::GetEditorState() == Playing` (`#if OPAAX_WITH_EDITOR`); release build hardcodes `true`. No production play-only subsystem exists yet — physics (M5) will be the first consumer.
- [x] `ViewportPanel::Draw(EEditorState)` — `ImGuiCol_Border` tint pushed (grey/green/amber). NOTE 2026-05-01: user reverted the explicit `WindowBorderSize=3.f` push in `ViewportPanel.cpp` — only the color is pushed now, so tint visibility depends on the default border size of the active style. Don't re-add the `WindowBorderSize` push.

### 3. Save / Open dialogs
- [ ] Vendor `tinyfiledialogs` under `Engine/Vendors/tinyfiledialogs/` (single header + .c, append to `Engine/CMakeLists.txt` via the existing vendor pattern around line 80).
- [ ] `SceneManager`: track current scene path (`OpaaxString m_CurrentScenePath`); expose `Save()`, `SaveAs(const char*)`, `Open(const char*)`, `IsDirty()` if cheap.
- [ ] **Open Scene…** → `tinyfd_openFileDialog` (`*.scene.json` filter) → `SceneManager::Open(path)`.
- [ ] **Save Scene** (Ctrl+S) → if `m_CurrentScenePath` empty, route to Save As; else `SceneManager::Save()`.
- [ ] **Save Scene As…** → `tinyfd_saveFileDialog` → `SceneManager::SaveAs(path)`.
- [ ] **New Scene** → confirm-discard modal if dirty → clear world + reset path.

### Verification
- [ ] Build clean (`debug-editor`).
- [ ] Toolbar renders; every menu item either works or shows a clear "TBD" tooltip.
- [ ] Save Scene As writes a `.scene.json`; Open Scene loads it; entities + parents + UUIDs intact.
- [ ] Ctrl+S triggers save; reflects "untitled → Save As" prompt when no path is set.
- [ ] Play → Pause → Stop transitions log; viewport border tint changes.
- [ ] Gameplay-tick gating: a system marked play-only does not tick in Editing state.

### Scope (out — deferred)
- Recent files persistence (would need a config slot + LRU list).
- Multi-scene tabs.
- Drag-drop a `.scene.json` from the Asset Browser onto the menu bar (the existing FIXME at `AssetBrowserPanel.cpp:273` lives in M3 territory).
- A real Undo/Redo command system (touches every editor mutation; out of M2).

---

## State Report (verified, condensed)

**Resolved**
- ✅ `CoreEngineApp::m_World` — orphan member deleted in M1.
- ✅ `OpenGLShader` uses Opaax `UnorderedMap` alias.
- ✅ `Camera2D` 1280×720 — superseded by `EngineConfig` window size (M1).
- ✅ Engine/Game config foundation — `engine.config.json` shipped (M1).
- ✅ Asset save/load symmetry — manifest-aware (M1).
- 🟦 `OpaaxMath` separate library — DROP. Pure GLM aliases.

**Still open** — items grouped into M2…M8 below.

---

## Milestone Roadmap

### ✅ M1 — Asset Unification + Config Foundation (DONE — see `tasks/done/2026-04-30_m1-asset-config.md`)

### M2 — Editor: Real Toolbar + PIE State + Save/Open  *(NEXT — see Active milestone above for details)*

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

## Pre-flight before resuming M2

- [x] Rotation+parenting verification complete (archive note 2026-04-30).
- [x] M1 verified & pushed.
- [x] `CLAUDE.local.md` "CURRENT MILESTONE" set to **M-02 — Editor: Real Toolbar + PIE State + Save/Open**.
- [x] Step 1 (Main menu bar) — done & pushed.
- [x] Step 2 (PIE state machine) — done & pushed (note: user disabled explicit `WindowBorderSize` push in `ViewportPanel.cpp`).
- [ ] **Next session**: read the **Active milestone — M2 §3 Save / Open dialogs** section above. Skip `/engine-planning`; drop straight into a plan-mode pass for Step 3. Critical files: `Engine/Source/Editor/Toolbar/MainMenuBar.cpp` (existing TBD callbacks), `Engine/Source/Scene/SceneManager.{h,cpp}`, `Engine/CMakeLists.txt` (vendor add).
