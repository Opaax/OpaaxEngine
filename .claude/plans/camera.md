# ① CAMERA — the core

> **CLOSED AND USER-VERIFIED, 2026-08-26.** Machine gates: three presets, tests **401 / 6807 / 7**,
> both hosts smoke clean. Interactive gates confirmed by the user, who named **pan, zoom, and PIE
> start/stop** explicitly and signed off the set as a whole. Commits `f56df60` `4cc3049` `a658ec3`
> `c97e9f6`, plus `baf988e` (their own Sandbox authoring). Contract: ARCHITECTURE.md **CAM1–CAM7**.
> Lessons: **L50** (a placement argument dies with the feature it rested on) · **L51** (guard a seed
> against the pre-measurement value).
>
> Block ① of `.claude/plans/engine-sequence.md`. That file holds the ORDER and the premises; this is
> the design and the steps. **Scope set by the user 2026-08-26: the CORE only** — no follow, no shake,
> no entity drag-drop, no priority or blending.

`RendererManager::RenderFrame` hard-codes a centred ortho (`RendererManager.cpp:145-151`) under a
comment promising a camera system would replace it. Nothing produces a `RenderView`. There is no way to
frame a level, and the editor viewport is welded to world origin. `RenderView.h` is already the seam and
already states the contract: *"the renderer has NO camera class; the host composes the matrices and
hands over a snapshot."* This block builds the thing that composes them.

---

## The design

```
                         writes                      reads
  CameraManager   ───────────────►  World::CameraView  ◄──────  RendererManager::RenderFrame
  (engine subsystem, Play worlds)     { Position,                 │  MakeViewProjection(view, w, h)
  EditorCamera    ───────────────►      OrthoSize }               ▼
  (editor-owned, Edit worlds)                                  RenderView { ViewProjection, Viewport }
```

**`CameraManager` is an ENGINE subsystem, not a world subsystem** (user's call, 2026-08-26, and they
were right). The world-subsystem shape was justified by *"it ticks behaviors"* — dropping follow deletes
that premise, leaving a resolve that is a pure function of the active world with **no per-world state**.
The engine tier costs one line in the `RegisterNativeSubsystems()` that already exists
(`Engine.cpp:81`), against a new `RegisterNativeWorldSubsystems()`, the first engine-native world
subsystem, a `WorldContext` it barely uses, and an instance per world including every test world. The
mode check becomes one honest `if` in one place instead of a `ShouldCreate`. **It survives follow
arriving later** — the smoothing is stateless and the camera's position lives on its entity in the
world's registry. ([[L47]]'s shape: a blocker I wrote down was a claim about PLACEMENT.)

**The resolved view stays on the `World`** — a separate question from the producer's tier. Three things
rest on it: it is per-world, which PIE needs (**WM6** — the Edit world and its clone are both live and
looked at differently); a world nobody wrote a view for keeps the **default-constructed** one, which is
today's exact framing, so the fallback is structural rather than a special case (**BO4c** one level
down, never a black frame); and it is the only seam the editor can write to without the engine ever
naming an editor type (**D4**).

**Projection is `OrthoSize` — the vertical half-extent in WORLD UNITS**, width derived from the target's
aspect. Today's `1 unit = 1 pixel` means a 4K player sees four times the playfield, a real bug for a
shmup. Default `300` reproduces today exactly at the viewport's 600px height, and `EditorCamera` seeds
its own size from the first viewport height it sees, so the editor opens on the framing it opens on now
at any panel size. **Accepted visible change:** dragging the viewport larger now scales the quads up
instead of revealing more map.

**Editor input comes from ImGui, and that is FORCED (`IN8`).** With an Edit world on screen the route is
`ClosedEditMode`, so `RouteInput` consumes every Input-category event and `InputManager` never sees a
button — an editor camera reading it would answer "nothing pressed" forever. The gesture is measured in
`ViewportPanel::DrawContents` where the window is current (the same source `Ctrl+S` uses), gated on
`IsWindowHovered/IsWindowFocused` and **never on `WantCaptureMouse`** — the viewport IS an ImGui window,
so that flag is always true over it ([[L29]]). **Middle-drag pans, wheel zooms at the cursor**; left and
right stay free for ②'s click-select and context menus.

---

## Files

**New** — `Renderer/CameraView.h`+`.cpp` (`struct CameraView { Vector2F Position; float OrthoSize =
300.f; }`, `MakeViewProjection`, `ScreenToWorld`) · `World/Components/CameraComponent.h` ·
`Engine/Subsystems/Camera/CameraManager.h`+`.cpp` · `Editor/Camera/EditorCamera.h`+`.cpp` ·
`Engine/Tests/Core/Renderer/CameraViewTests.cpp`.

**Modified** — `World.h` (a `// ==== Camera` section) · `RendererManager.cpp:145-151` · `Engine.cpp`
(both native registrations) · `EditorContext.h` · `EditorService` · `ViewportPanel` ·
`SandboxEditorModule.cpp` (one drawer line) · `Engine/Tests/CMakeLists.txt` · `RenderView.h`'s comment.

**Deleted (S4)** — `Legacy/Renderer/Camera/` (16 files) + `Legacy/Editor/Camera/` (2). Verified **zero
live references** outside `Legacy/` and both trees are unglobbed (**X1**), so it cannot break a build.

**Salvage, not reinvention.** `MakeViewProjection` is `OrthographicCamera.cpp:68-88` minus the
dirty-flag caching; `ScreenToWorld` is `OrthographicCamera.h:51-59`; `EditorCamera::Pan`/`ZoomAtCursor`
are `Legacy/Editor/Camera/EditorCamera.cpp:18-59`. What does NOT come is the `ICamera` /
`ICameraController` hierarchy around them — it fights **I8** and **D7**.

---

## Steps (gates live in `.claude/task/todo.md`)

- **S1 — the seam + the fallback.** `CameraView` + the two exported free functions, the `World` slot,
  `RendererManager` composing from it, the math tests. No camera types yet.
  **Bodies out of line and `OPAAX_API`:** inline would drag `glm/gtc/matrix_transform.hpp` into every
  TU including `World.h`, and the editor calls `ScreenToWorld` **from the exe** — **I6**'s tell, which
  has produced `LNK2019` three times here. Exported ahead of the break for once.
- **S2 — `CameraComponent` + `CameraManager`.** `_WITH_DEFAULT` (**I8**) + `OPAAX_PROPERTIES`
  (**I15**); `Startup` caches `WorldManager*` through `IEngine` (**F3**, as `RendererManager.cpp:94`);
  `Update` early-outs unless the active world is `Play`, then first `CameraComponent` wins.
  Registration order is free — `Engine::Loop` runs `UpdateAll` and `RenderAll` as separate passes
  (`Engine.cpp:420/429`).
  **The authoring path costs zero editor code:** the Inspector's `Add Component` popup walks
  `ComponentRegistry` (`InspectorPanel.cpp:106-119`), so `Camera` appears there the moment it is
  registered natively — no hand-edited json, no **MP6** risk.
  **Logging ([[L48]]):** report on TRANSITION, keyed by `World::GetId()` + count, never per frame —
  camera found / none found (Warn) / several (Warn naming the count, first wins).
- **S3 — `EditorCamera`.** One instance owned by `EditorService`, carried on `EditorContext` (the
  `Selection`/`PIE`/`Route`/`Preview` shape) — which is what makes pan/zoom survive PIE **by
  construction**, the same reason legacy's lived on `EditorSubsystem`. `ViewportPanel::DrawContents`
  measures, `OnPreRender` applies before `Loop`. `Apply` early-outs unless `Mode == Edit`: **one
  statement of the rule, one place** — the whole of "two cases, not three".
- **S4 — delete Legacy, fix the contract.** ARCHITECTURE.md gains a `CAM` section and **I6** a fourth
  strike; `RenderView.h:25` stops citing a deleted `ICamera`; `Editor.md` D5's "the editor camera …
  today that branch consumes input and drops it" becomes false; `engine-sequence.md` §③ becomes three
  components.
  **Salvage recorded before the delete** so a later block can `git show` instead of rediscovering:
  `FollowCameraController.cpp:16-52` (deadzone → snap → frame-rate-independent exponential smoothing);
  `ShakeParams.h` + `ShakeCameraController` (amplitude/frequency/duration/decay, decoupled Y ratio and
  phase); `ScreenSpaceCamera.h` (pixel-space Y-up HUD view — ⑥'s text and HUD).

---

## Not built, named deliberately ([[L23]] — never an API with no caller)

Follow · shake · priority and blending (**several cameras: first wins and it warns**; a `Priority` field
nothing reads is a spec, and **X5** deletes those) · `ViewportRect` / multi-view (⑥ owns it; `RenderView`
already promises it is nearly free) · perspective · confiner/bounds · a scene view that detaches from
the game camera during PIE · screen→world **picking** (`ScreenToWorld` lands here because zoom-at-cursor
needs it; turning it into click-select is ②).

## Risks named up front

- **`CameraComponent.Position` is debt on purpose** — the third component in ③'s fold, carrying the same
  standing comment `SpriteComponent.h:26-29` does. The alternative was building Transform now, which the
  sequence deliberately refused.
- **`World.h` gains a `Renderer/` include.** No cycle (`CameraView.h` sees only Core), and `WorldContext`
  already holds a `DebugDraw&` from that module — but it is a new direction, so it is said out loud.
- **The `OrthoSize` switch is the one thing that changes how the editor LOOKS** at any viewport height
  other than 600px. Seeding from the first height hides it at launch; resizing reveals it.
- **`CameraManager` and `EditorCamera` are both Legacy names until S4 lands**, so a grep during S2–S3
  answers two files with no lineage between them (**X4**). Do not reorder S4 behind anything.
