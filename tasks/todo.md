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
- [x] Vendor `tinyfiledialogs` under `Engine/Vendors/tinyfiledialogs/` (single header + .c, editor-gated `add_library` block in `Engine/CMakeLists.txt` after the `glad` block, Win32 link libs `comdlg32 ole32 user32 shell32`).
- [x] `SceneManager`: tracks current scene path (`OpaaxString m_CurrentScenePath`); exposes `Save()`, `SaveAs(const char*)`, `Open(const char*)`, `NewScene()`, `HasCurrentScenePath()`, `GetCurrentScenePath()`. **`IsDirty()` deferred** — would need per-mutation hooks; modal always confirms instead.
- [x] **Open Scene…** → `tinyfd_openFileDialog` (`*.scene.json` filter) → `SceneManager::Open(path)`. Reuses active scene (`World::Clear()` + `Deserialize`) to preserve the derived `GameScene` instance.
- [x] **Save Scene** (Ctrl+S) → routes to `DoSaveOrSaveAs`: if `m_CurrentScenePath` empty, calls Save As dialog; else `SceneManager::Save()`. Wired via `ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)` in `MainMenuBar::Draw`.
- [x] **Save Scene As…** → `tinyfd_saveFileDialog` → `SceneManager::SaveAs(path)`.
- [x] **New Scene** → confirm-discard modal (mirrors About-modal pattern) → `SceneManager::NewScene()` (clears world + resets path).

### Verification
- [x] Build clean (`debug-editor`) — only pre-existing `APIENTRY` redef + `LIBCMT` warnings; `tinyfiledialogs.lib` produced; `OpaaxEngine.dll` + `Game.exe` linked.
- [ ] **Manual UI checks (run `Game.exe` from `build/debug-editor/bin/Debug/`)**:
    - [x] Toolbar renders; every menu item works or shows a clear disabled state.
    - [x] Save Scene As writes a `.scene.json`; Open Scene loads it; entities + parents + UUIDs intact.
    - [x] Ctrl+S triggers save; with no path set, opens the Save As prompt.
    - [x] New Scene → modal confirms; Discard wipes world; Cancel keeps everything.
    - [ ] Play → Pause → Stop transitions log; viewport border tint changes (regression).
    - [ ] Gameplay-tick gating: a system marked play-only does not tick in Editing state (regression).

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

### M2.5 — Editor Project Paths & Dialog UX  *(POST-M2 polish, blocks comfortable scene authoring)*

> **Why now:** M2 Step 3 wired native Save/Open dialogs, but `OpaaxPath::Resolve` is exe-relative (`build/debug-editor/bin/Debug/`). The dialogs default to that bin folder — wrong location for scene files we want versioned with sources. Also formalises a "Project Root" abstraction we'll need for thumbnails (M8) and any source-tree-aware tooling.

- [ ] Inject `-DOPAAX_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"` from the top-level `CMakeLists.txt` as a compile definition (editor builds only — release shouldn't bake source paths).
- [ ] `OpaaxPath::GetProjectRoot()` — new static returning the injected path; `s_ProjectRoot` populated in `Init()` next to `s_BasePath`. Falls back to `s_BasePath` when undefined.
- [ ] `EngineConfig` JSON: add `editor.defaultScenePath` (default `"Game/Assets/Scenes"`) — directory the dialogs default to when `m_CurrentScenePath` is empty. Resolved via `GetProjectRoot()`.
- [ ] `MainMenuBar::DoOpen` / `DoSaveAs` — pass the resolved default as `aDefaultPathAndFile` to `tinyfd_*`. Trailing slash matters on Windows: `tinyfiledialogs` interprets paths without an extension as a directory only if they end in `/` or `\`.
- [ ] `EditorSubsystem` — remember the last-used dir across dialogs (volatile member; no persistence). After a successful Open/SaveAs, store the parent dir; next dialog defaults to it instead of the engine default.
- [ ] Optional polish: status-bar label in `MainMenuBar` showing the current scene path relative to project root (e.g. `Scene: Game/Assets/Scenes/Test.scene.json` or `Scene: <untitled>`). Use `OpaaxPath::MakeRelative` against `GetProjectRoot()`.
- [ ] Verification:
    - [ ] First Save As opens at `Game/Assets/Scenes/`, not `bin/Debug/`.
    - [ ] After a save, opening Save Scene As again defaults to the same folder.
    - [ ] Release build still configures + links cleanly (project-root define is editor-gated).
    - [ ] `OPAAX_PROJECT_ROOT` is **not** present in any release-mode binary (grep the strings table or accept a build-time check).

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
- [x] Step 3 (Save / Open dialogs) — built clean on `debug-editor` 2026-05-02. **Pending: manual UI verification + commit/push.** Plan archived at `C:/Users/engue/.claude/plans/mighty-percolating-finch.md`.
- [x] Step 3 follow-up fix (2026-05-02) — removed implicit shutdown auto-save in `CoreEngineApp::OnShutdown`; gated `GameScene::OnUnload`'s SaveScene with `#if !OPAAX_WITH_EDITOR`. Editor mode no longer overwrites `GameScene.json` at quit.
- [ ] **Step 3 follow-up bug (reported 2026-05-02)**: after Save Scene As… writes a new `.scene.json`, the file does not appear in the Asset Browser until the editor is restarted (no live refresh). Likely the AssetBrowserPanel scan only runs at startup. Fix options: trigger an `AssetScanner::Rescan()` (or equivalent) on successful `SceneManager::SaveAs`, or make `AssetBrowserPanel` watch its target dir / poll on focus. Pick the cheaper path for M2; full filesystem-watch lives in M8.
- [ ] **Step 3 follow-up UX (reported 2026-05-02)**: Asset Browser does not surface which scene is currently loaded. Hovering / inspecting the entry that matches `SceneManager::GetCurrentScenePath()` shows nothing distinctive — no badge, no tooltip, no highlight. Fix: in `AssetBrowserPanel`, when rendering a `.scene.json` entry, compare its path against the active `SceneManager::GetCurrentScenePath()` and apply a visual marker (bold label, leading dot, accent tint, or "(loaded)" suffix). Add a tooltip on hover. Cheap and self-contained.
- [ ] **Step 3 follow-up bug — `.scene.json` poisons the manifest on reload (reported 2026-05-02)**: `std::filesystem::path::extension()` returns only the last extension, so `Test.scene.json` yields `.json`. `AssetScanner::ResolveType` then maps it to type `Scene` (correct), but `AssetScanner::Scan` only skips when type == `Unknown` — so scenes get **added to the asset manifest** despite the doc comment at `AssetScanner.h:27` claiming "Scenes/ → skipped (managed by SceneManager)". `GenerateID` strips only `.json`, leaving IDs like `Scenes/Test.scene` (broken — `.scene` still attached). On next engine launch the manifest already contains these entries; AssetRegistry tries to reconcile them and the editor shows an inconsistent state. **Fix**: honor the doc comment — make `Scan` skip `lType == "Scene"` entries entirely (don't write them to the manifest). Scenes belong to `SceneManager`, not `AssetRegistry`. Bonus: `GenerateID` should strip the full multi-extension when the type is Scene, but if scenes are skipped this becomes moot.
- [ ] **Next session**: run the manual UI checklist (Save As → Open round-trip, Ctrl+S, New Scene confirm, PIE regression, **verify quitting the editor no longer clobbers `GameAssets/Scenes/GameScene.json`**, and the asset-browser refresh follow-up bug above). If green, commit Step 3 and archive `tasks/done/2026-05-02_m2-step3.md`. After M2 closure, candidate next: **M2.5 (paths/dialog UX)** — confirm with user.
