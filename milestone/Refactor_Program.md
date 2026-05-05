# Refactor Program — Opaax Engine

> Top-level pointer doc. Each phase = one milestone. M0 / M1 are detailed; M2–M7 are sketched and detailed when they become next-up.

---

## Program Goal

Refactor Opaax Engine to lift its weakest subsystems (renderer extensibility, world/scene, asset pipeline, allocators, camera, editor coupling) without throwing away the working core (Subsystem, Event, StringID, Path, Config, Component/Entity, editor panel/drawer/asset-action infrastructure). The engine must remain runnable at every step.

---

## Decision Log

- **2026-05-05** — Refactor (not restart). Rationale: ~60–70 % of engine plumbing is sound; weak items are localized; restart kills momentum and rediscovers the same problems 8 months later. See architectural audit (conversation history).
- **2026-05-05** — Plan deeply for what's next, sketch what's far. M0 / M1 are fully spec'd; M2–M7 are stubs and will be detailed when their predecessor enters `Active`.
- **2026-05-05** — Naming / prefix policy: decision deferred to **end of M0** so we have surveyed every offender before committing.

---

## Phases

| ID | Name | Status | Estimate | Plan |
|----|------|--------|----------|------|
| M0 | Hygiene Sweep | Active | Low (~1 wk) | [M0_Hygiene_Sweep.md](M0_Hygiene_Sweep.md) |
| M1 | Asset Foundation | Planned | Medium (~2–3 wks) | [M1_Asset_Foundation.md](M1_Asset_Foundation.md) |
| M2 | World / Scene Consolidation | Sketch | Medium (~2 wks) | (stub below) |
| M3 | Editor Event Plumbing | Sketch | Low (~1 wk) | (stub below) |
| M4 | Renderer Pass System | Sketch | High (~3–4 wks) | (stub below) |
| M5 | Camera | Sketch | Medium (~1 wk) | (stub below) |
| M6 | Memory Allocators | Sketch | Medium (~1–2 wks) | (stub below) |
| M7 | Asset Browser Polish | Sketch | Low (~1 wk) | (stub below) |

---

## Sketches (M2–M7)

### M2 — World / Scene Consolidation
Pick one ownership model. Recommendation: `World` owns `entt::registry` + `SceneManager`; `Scene` is a serializable unit; merge `Core/World/` and `Scene/` into one tree; move `WorldRendererSystem` out of `Core/World/` into `Renderer/` as an extractor; introduce `IWorldSystem` for ticking systems. Resolves the dead `CoreEngineApp::m_World` flag from `engine-planning.md`.

### M3 — Editor Event Plumbing
Define `OnEntitySelectedEvent`, `OnSceneSavedEvent`, `OnNewSceneEvent`, `OnAssetImportedEvent`. Replace every panel-to-panel call with publish/subscribe via `OpaaxEventDispatcher`. Split `IEditorPanel` type-ID from instance-ID — multi-Inspector becomes possible. Hard rule: no panel ever holds a raw pointer to another panel.

### M4 — Renderer Pass System
Introduce `IRenderPass`, `RenderQueue`, `RenderView`. Extract current `Renderer2D` logic into `SpritePass`. Add `DebugDrawPass` and `EditorGridPass` to prove extensibility. Resolves: `Renderer2D s_Data` static-global thread-safety; opens the path to a non-OpenGL backend later. Largest phase. **Escape valve:** if coupling is so deep that the pass system requires touching ≥ 60 % of non-renderer files, consider a *scoped* rebuild of `Renderer/` only on a side branch — not a full restart.

### M5 — Camera
Split `ICamera` (data + matrices) from `ICameraController` (behavior). Lift `Camera2D` behind `OrthographicCamera`. Build `FollowCameraController` and `ShakeCameraController` for the shmup / platformer targets. Resolves the hardcoded 1280×720 viewport — viewport becomes per-camera.

### M6 — Memory Allocators
`LinearAllocator` (frame scratch) + `PoolAllocator<T>` first. Wire `FrameAllocator` into renderer command buffers (built in M4). Validate `PoolAllocator` against a sample bullet/particle pool. `StackAllocator` and `FreeListAllocator` deferred until profiler asks. Late on purpose: allocators only pay off once there's a working, working-game-shaped engine to profile.

### M7 — Asset Browser Polish
Folder tree backed by `AssetScanner` over project-relative `OpaaxPath` (already fixed in M0). Drag-and-drop and context menus through `IAssetTypeAction`. Rename / move propagates through the asset registry.

---

## Out of Scope

Items flagged in `engine-planning.md` and elsewhere that are NOT part of this refactor program: Physics, Audio, Job system / threading, `SpriteSheet` / `Animation` components, Toolbar UX, editor asset icons, Play↔Viewport linking, `OpaaxString` printf-style format, Vulkan backend, engine math sizing review. See [Backlog.md](Backlog.md).

---

## How To Use This Plan

1. Read [`.claude/task/todo.md`](../.claude/task/todo.md) for the active checklist.
2. Read the active milestone's plan file (linked from `.claude/CLAUDE.local.md`).
3. When a milestone closes:
   - Move it to `Status: Done` in the table above.
   - Promote the next milestone to `Active`. If it's still a sketch, write its full plan now.
   - Update `.claude/CLAUDE.local.md`.
   - Capture lessons in `.claude/task/lessons.md`.
