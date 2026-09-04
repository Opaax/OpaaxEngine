# ⑥ S5 — MULTI-VIEW

## Context

Block ⑥'s last item. `RenderView` has promised it since M1 — *"One BeginPass per view, so
split-screen / editor viewport / minimap are just more views"* — and **F5** made it correct by
sorting per PASS rather than per frame. What was never built is a frame composing more than one:
`RendererManager::RenderFrame` hardcodes a single bracket into `m_PrimaryTarget`.

**The consumer question is what shaped this block.** Multi-view alone cannot be verified: with one
pass still composed, the frame renders byte-identically and no smoke run, log line or test can tell
the change from no change ([[L23]] / **X5**). And every cheap consumer has a cheaper answer that is
not multi-view — a camera-framing rectangle is `DebugDraw`; asset previews are ImGui images the clip
editor already draws; the selection outline's zoom-thinning is `WorldPerPixel()`, which the grid
already uses. So the block is defined by its consumer, not by the plumbing.

**Decisions locked with the user (2026-09-04):**

| | Decision |
|---|---|
| Consumer | **A Camera Preview panel**, opened by a button on `CameraComponent`. READ-ONLY, driven by the camera entity's own settings — so it never writes `World::SetCameraView` and **CAM1**'s single slot is untouched. That is what makes it cheap where a second *editing* viewport is not. |
| HUD | **NOT in this block.** *"hud I want it in certain way its demand design yet"* — the render half is built to accept one; anchors, reference resolution and the authoring surface stay theirs to design. |
| Stats overlay | **Dropped.** *"We have stats panels, for now is very ok."* It is no longer BLOCKED on multi-view (**TX10**'s wording) — it is deferred by them. |
| The button | **Dispatches a command** (*"Just call a command"*), never inline work. |
| Drawer contract | **A custom drawer may receive `EditorContext&`** (*"Maybe its good to have editor context for drawer too. some custom drawer may need it more often"*). |
| Empty state | **Unity's "No camera"** — a panel that vanishes is confusing. |
| Stacking | **No.** *"i do not really like the stack."* One panel, following the selection. |

## Four things the design does NOT need, and why

Proposed across the design conversation, then deleted by the decisions above. Recorded so they are
not re-proposed:

- **A `ComponentActions()` registry** — rejected as machinery (*"cant we make it simplier"*). A
  custom drawer plus the context does the same job with no new route and no new seal-line counter.
- **A new command + tag** — `EDITOR_COMMAND_TOGGLE_PANEL` already exists and takes a `PanelIdParams`
  payload (*"ONE tag for every panel; which one is the payload"*).
- **A `CameraPreview` state object** on `EditorContext` (the `ResourcePreview` shape) — the panel
  follows the SELECTION, and the Inspector is by definition drawing the selected entity, so there is
  no "which camera" to carry.
- **CAM1 rework** — a second *editing* viewport would have two `EditorCamera`s fighting over the
  world's one `CameraView` slot. A read-only preview reads the camera entity instead and writes
  nothing.

## The render shape

`RendererManager::SetPrimaryRenderTarget(IRenderTarget*)` becomes per-frame submitted views:

```
SubmitView(IRenderTarget&, const CameraView&, bool bDrawOverlays)
```

- **Submitted in `OnPreRender`, drained by `RenderFrame`, cleared every frame — F4's idiom.** A
  producer that wants a view re-submits it; nothing is retained. The panel that owns the target is
  the panel that submits it, so a hidden or destroyed panel stops costing a pass by construction.
- **The adapter composes the matrix against THAT target's pixels**, so **CAM1**'s split holds: the
  panel says where it is looked at from, the adapter is what knows pixels.
- **An empty list falls back to backbuffer + the active world's `CameraView`** — today's exact
  behaviour, one line rather than a branch to keep in step (**BO4c**'s never-a-black-frame rule).
  `Sandbox.exe` renders byte-identically and never learns any of this exists.
- **`bDrawOverlays` has two callers on day one** — true for the Viewport (grid, selection outline,
  entity icons), false for the Camera Preview, which must look like the GAME. That is the rule
  `EnqueueEntityIcons` already states for Play worlds, not a new one. Not a field nothing reads
  (**X5**).
- **`RenderSystem::BeginPass` takes an `ELoadOp`.** Two passes into one target need `Load` on the
  second; two passes into two targets are both `Clear`. **`ELoadOp::Load` has zero callers today** —
  this is its first, and `ICommandBuffer.h` already names the case in its own comment
  (*"composite-on-top passes like the overlay"*).

## The editor shape

- **`CameraPreviewPanel`** owns an `IFramebuffer` + `OffscreenRenderTarget` and copies
  `ViewportPanel`'s deferred-resize shape (measure in `DrawContents`, apply in `OnPreRender`). In
  `OnPreRender` it resolves the selected entity's `TransformComponent` + `CameraComponent` into a
  `CameraView` — the same pair `CameraManager::Resolve` builds — and submits it.
- **No camera resolved is a first-class state**, not an error: the panel draws "No camera" and
  submits no pass. Covers a deselection, a non-camera selection, a deleted entity, and a PIE clone
  whose ids do not carry.
- **`TDrawerRegistry` gains an optional `Draw(IEditorWidgets&, T&, Entity&, EditorContext&)` form**,
  detected with `if constexpr` — the same duck-typing the registry already uses to tell its generic
  form from its custom one (*"DUCK-TYPED contract, checked at instantiation, with NO base class"*).
  The subject is already in the closure; the context comes down from the Inspector, which holds it.
  **The cost, stated once:** any drawer can then reach the whole editor, and what stops one abusing
  it is convention rather than the type system. Accepted deliberately — the next component that
  wants "Convert to Sheet" or "Focus this" (`Docs/TODO.txt`) needs the same thing.
- **`CameraComponentDrawer`** = its own `CollapsingHeader` (the custom form does not frame the
  drawer) + `DrawProperties(InWidgets, InCamera)` — the same free function the generic form folds,
  so the button is literally *after the default props* — + a `Button` that dispatches
  `EDITOR_COMMAND_TOGGLE_PANEL`. `Register<CameraComponent>()` becomes
  `Register<CameraComponent, NativeComponentDrawers::CameraComponentDrawer>()`.

## Steps — each stops for their check

**S5.1 — THE PASS LIST.** `SubmitView` + the `RenderFrame` loop + the empty-list fallback + the
per-pass `ELoadOp`. `ViewportPanel` becomes the first submitter, replacing
`SetPrimaryRenderTarget`. A one-shot Info line naming the pass count the first time it exceeds 1
([[L12]]/[[L15]] — a smoke run cannot read the Stats panel).
*Gate:* `Sandbox.exe` and `SandboxEditor.exe` look identical and the log is unchanged. **Nothing is
visible yet, and that is the expected result** — this step is the seam, not the feature.

**S5.2 — THE PANEL.** `CameraPreviewPanel` + its FBO, following the selection, with the "No camera"
state. Registered like every other panel; reachable from the Window menu.
*Gate:* open it beside the Viewport, select the Sandbox camera, see the GAME's framing next to the
editor's; the one-shot pass-count line fires; `Draw Calls` roughly doubles.

**S5.3 — THE BUTTON.** The `TDrawerRegistry` context overload + `CameraComponentDrawer` + the
registration swap.
*Gate:* select a camera → the Inspector shows `OrthoSize` and then the button → click → the panel
opens on that camera → move the entity or edit `OrthoSize` and the preview tracks live.
**THEIR EYES OWN THIS ONE** — a smoke run never opens the Inspector or clicks a button ([[L64]]).

Counters: `panels=13→14`, `drawers=8` unchanged (a registration changes form, not count).

## Not in scope, and why

- **HUD** — theirs to design (locked above). The render half accepts it: a HUD is one more submitted
  view, into the same target, with `ELoadOp::Load` and a pixel projection.
- **Split-screen, minimap** — no caller (**X5**).
- **A second EDITING viewport** — needs **CAM1**'s slot to move off the World; the preview
  deliberately avoids that, and it is the change to make when a real second author's-eye view is
  wanted.
- **The asset preview WORLD** (Unreal's preview scene) — Unreal's answer to a 3D problem this engine
  does not have: 2D previews are images, and `AnimationClipPanel` already plays one through ImGui on
  its own clock. **Its trigger is ⑦ prefabs** — a composed entity is the first preview an image
  cannot fake. Second trigger: particles, or a material/shader asset.
- **Pinning the preview to an entity** — needs the `CameraPreview` state the selection made
  unnecessary. Trigger: wanting two cameras previewed at once, which is also when the stack returns.

## Expect, do not "fix"

- **`Draw Calls` roughly doubles while the preview is open** and the Stats panel's amber row trips.
  A split is a COST, not a drawing error (**ST7**) — and here it is two passes doing their job.
- **The preview empties when a non-camera entity is selected.** That is Unity's behaviour and the
  agreed design, not a lost binding.
