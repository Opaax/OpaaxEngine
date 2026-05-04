# Todo — OpaaxEngine Milestone Roadmap

> Source plan: `C:\Users\engue\.claude\plans\silly-petting-charm.md`
> Audit date: 2026-04-30 — verified against current code via three parallel Explore agents.
> Archives:
>   - `tasks/done/2026-04-30_rotation-parenting.md`
>   - `tasks/done/2026-04-30_m1-asset-config.md`
>   - `tasks/done/2026-05-01_m2-step1-step2.md`
>   - `tasks/done/2026-05-04_m2-step3-and-followups.md` *(M2 Step 3 + F1/F2/F3 + M2.5)*

## Status legend
- [ ] pending
- [~] in progress
- [x] done

---

## Active milestone — M2 Editor: closure

> **State:** All implementation work shipped & pushed. Two PIE manual regression checks remain — non-blocking, run at your cadence.

### Remaining
- [ ] Manual UI check: Play → Pause → Stop transitions log; viewport border tint changes (grey/green/amber).
- [ ] Manual UI check: Gameplay-tick gating — a system marked play-only doesn't tick in Editing state. Note: nothing production currently uses `IsPlayOnly()`; this is a state-machine sanity check until physics (M5) lands.

Once both green: M2 is fully closed. The next active milestone is **M3 — Sprite UX** (or revisit the asset-pipeline-as-Unreal/Unity question first; user flagged that during M2.5 follow-ups).

---

## State Report (verified, condensed)

**Resolved**
- ✅ `CoreEngineApp::m_World` — orphan member deleted in M1.
- ✅ `OpenGLShader` uses Opaax `UnorderedMap` alias.
- ✅ `Camera2D` 1280×720 — superseded by `EngineConfig` window size (M1).
- ✅ Engine/Game config foundation — `engine.config.json` shipped (M1).
- ✅ Asset save/load symmetry — manifest-aware (M1).
- ✅ M2 Step 3 + Save/Open dialogs + tinyfiledialogs (2026-05-04 archive).
- ✅ Asset Browser auto-refresh after Save Scene As (F1).
- ✅ Current scene marked as Loaded in Asset Browser (F2).
- ✅ Manifest poisoning — `GenerateID` strips compound extensions; `ResolveType` by filename pattern (F3).
- ✅ Project root + `editor.defaultScenePath` + last-used dialog dir (M2.5).
- 🟦 `OpaaxMath` separate library — DROP. Pure GLM aliases.

**New since M2 opened — open questions, not yet scheduled**
- 📌 **Editor event system** — `OnSceneSaved` / `OnEntitySelected` / `OnNewScene` to replace explicit cross-panel refresh calls. Memory: `project_editor_event_system`.
- 📌 **Asset pipeline parity with Unreal/Unity** — paused on AssetBrowserPanel-doesn't-show-source-tree-files. Right answer is a real asset-pipeline pass (source-of-truth in source tree, build copies cooked, project-root manifest). Earns its own milestone. Revisit before M3 if it blocks authoring.

---

## Milestone Roadmap

### ✅ M1 — Asset Unification + Config Foundation (DONE — see `tasks/done/2026-04-30_m1-asset-config.md`)

### ✅ M2 — Editor: Real Toolbar + PIE State + Save/Open (DONE — see `tasks/done/2026-05-04_m2-step3-and-followups.md`; closure pending only the two PIE manual checks above)

### ✅ M2.5 — Editor Project Paths & Dialog UX (DONE, bundled into the M2 archive)

### M3 — Sprite UX: Intrinsic Size + SpriteSheet + Animation  *(NEXT candidate)*

> Closes the "transform scale unpredictability" complaint and gives 2D content authors first-class atlas + animation workflows.

- [ ] Add `SpriteComponent::Size` (world units) decoupled from `Transform.Scale`. Final draw size = `Size × Transform.Scale` composed through hierarchy.
- [ ] New `SpriteSheetComponent` — atlas reference + grid (`cellW`, `cellH`, `columns`, `cellIndex`).
- [ ] New `SpriteAnimationComponent` — frame list, timing, loop. Drives `cellIndex` on the sibling `SpriteSheetComponent`.
- [ ] Inspector drawers + serialization for the two new components.
- [ ] Critical files: `ECS/Components/SpriteComponent.{h,cpp}`, new `SpriteSheetComponent`, new `SpriteAnimationComponent`, `Renderer/Renderer2D.cpp` (sprite size param), `Editor/Inspector/Drawers/`, `WorldRenderSystem.cpp`.
- [ ] Verification: a sheet-driven character animates in editor; nested-parent scaling produces predictable on-screen size.

> **Pre-flight before M3**: confirm with user whether the asset-pipeline question (📌 above) blocks M3 authoring. If yes, do that first as M2.6 / new milestone.

### M4 — RHI Cleanup (prep for Vulkan)

- [ ] Move resource factories behind `IRenderAPI`: `CreateBuffer`, `CreateShader`, `CreateTexture2D`, `CreateVertexArray`.
- [ ] Wrap `Renderer2D::s_Data` (`Renderer2D.cpp:57`) in a `Renderer2DContext` instance owned by the renderer; mutex command submission.
- [ ] Backend stays GL-only; the seam is the deliverable.

### M5 — Physics 2D (Box2D)

- [ ] Vendor Box2D as a submodule; `PhysicsSubsystem` ticking on the existing `FixedUpdateAll` slot (`CoreEngineApp.cpp:177`).
- [ ] Components: `Rigidbody2DComponent` (Static/Kinematic/Dynamic), `BoxCollider2DComponent`, `CircleCollider2DComponent`.
- [ ] Two-way sync with `TransformComponent`; respect parent hierarchy (only root entities own bodies; children ride their parent transform).
- [ ] Inspector + serialization.
- [ ] First production consumer of `IsPlayOnly()` — finally exercises the gameplay-tick gate added in M2 Step 2.

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
- [ ] Editor event system (📌 above) — schedule here unless an earlier milestone forces it.
- [ ] Filesystem-watch on the asset dirs (today's manual Refresh button + `RefreshAssetBrowser()` shim cover the immediate need).

---

## Scope explicitly OUT of M2 (deferred)

- Recent files persistence (config slot + LRU list).
- Multi-scene tabs.
- Drag-drop `.scene.json` from Asset Browser onto menu bar (FIXME at `AssetBrowserPanel.cpp:294`).
- Real Undo/Redo command system.
- Status-bar label showing current scene path (M2.5 stretch).
