# UI — record (branch `User_Interface`, started 2026-09-14)

| Step | Commit | What landed |
|---|---|---|
| **U1** | `276b905` | `Engine/Source/UI/`: `UIRect` + `ResolveRect`, `UIWidget` (3 flags, 2 verbs, one walk), `UICanvas` (view = `CameraView{0, H/2}`, stats), `UIPanel`, `UIImage` (fill). 16 cases, **810 → 826 / 9029 → 9114**. No caller yet. |
| **U2** | `6578fea` | The canvas over the world: `bDrawUI` opt-in on the world pass, `SubmitUICanvas`, `RenderCanvases` with **`ELoadOp::Load`'s first caller**, ST rows. `Text2D` box (wrap/align, scan-then-emit). `IUIFontProvider` + the re-arm. `UIText`. `UISubsystem` tenant + `WorldContext::UI`. Sandbox `HudSubsystem` (Jumps + speed bar). 10 cases, **826 → 836 / 9114 → 9161**. Contract **§UI** (UI1–UI8). |

**Settled in U2, not in the seed:** the UI is NOT a view of its own — the runtime fallback keys on
an empty list, so it rides the world pass, opt-in (UI5) · layout happens at RENDER time, per
target (UI5) · the face is a string path through a provider the renderer implements (UI6) · a
`Rebuild` may re-arm, which is how an uploading atlas is waited for with no polling (UI3) ·
"score + health" became **Jumps + speed** — the player has no health and the gun does not fire;
both are real. **Not proven by a smoke run:** the picture itself, and a resize keeping the
corners — their eyes. `Sandbox.exe` opens `Main`, which has no mover, so its bar stays empty
there; `PhysicsTest` in PIE is where both move.

**Settled in U1, not in the seed:** a resolve that lands on the SAME rect stops propagation below
it (a corner-anchored child of a widening root costs one resolve, its subtree nothing) · the root
is `bHitTestable = false` and `UIPanel` defaults to pass-through, so "a widget was hit" can never
mean "the pointer is on screen" · visibility and hit-testability dirty nothing — they are read at
submit / hit-test time · `RemoveChild` hands the node back (undo will want it) and dirties nothing.

---

## The seed (2026-09-14, pre-branch)

> The block after parenting. Their first notes + four decisions, and the shape those decisions
> settle. Noesis is a **reference for shape only** (a UI core that knows nothing about the game;
> ViewModels bound onto it) — **no vendor**, their call. Unity's one-canvas simplicity and Unreal's
> editing are the two other references they named.
> Legacy salvage: `Legacy/World/IOverlayRenderSystem.h` (an overlay pass after the world pass, into
> the same target, no clear — the same shape as MV's named HUD growth point) and `WorldOld.h`'s
> persistent HUD entity — the problem this block solves at a different tier.

## Their decisions (2026-09-13/14)
1. **UI is DECOUPLED from World.** Not entities. Its own object model in its own module.
2. **No vendor.** Noesis is a good reference, nothing more.
3. **Sequence: the core UI system first; the consuming systems after** — the GameInstance tenant,
   any world-side hosting, the editor panel, HUD bindings are all *consumers* of the core.
4. **Async loading will come.** The loading screen is a cover today and must become a progress
   screen later **without the UI changing** — so the UI must tick and render with no world at all.
5. **A canvas is expensive because of resizing etc. — handle it correctly.** Invalidation-driven
   layout; resize as projection first; a number in the Stats panel that reads 0 when idle.

## The shape

### Module — `Engine/Source/UI/`, depends on Core + Renderer, never World
- Consumes `Renderer2D`, `Text2D::Layout` (**TX14**'s sink idiom — the layout is pure, the renderer
  is a sink), `CameraView`, `DebugDraw`. **Never** `World`, `WorldManager`, entt, a subsystem.
- **Testable headless** for the same reason **TX**'s walker is: rects, invalidation, hit-testing
  and text caching are pure; the GL half is one submit loop.
- The hosts own canvases and call three verbs: `Layout`, `Render(Renderer2D&)`, `HitTest(point)`.

### Object model — a widget tree, `CReflected`
- `UIWidget` — owned children, a `UIRect`, visibility, two dirty flags. **`CReflected`, so
  `DrawProperties` draws it unchanged**: the property system already serves non-entity documents
  (config, library entries, mover tunings), which is what makes the editor panel cheap.
- `UIRect` — Unity's RectTransform fields: anchor min/max (0..1 of the parent rect), pivot,
  anchored position, size delta. The one data structure everything rests on; the layout is a rect
  walk down the tree (`ComposeChain`'s shape with rects). *Unreal's slot-based model considered;
  Unity's is the one their notes cite and the one an author already knows.*
- `UICanvas` — the root. Reference height, the tree, the ordering. Owns nothing else.
- Leaves: `UIImage` (Simple / **Sliced** 9-slice / **Filled** — the health bar), `UIText`
  (wrap + alignment, **TX10**'s "no caller" items — the UI is the caller; caches its `TextQuad`s),
  `UIButton` (states + `OnClick` = `TMulticastDelegate`), `UIMask` (rect clip), `UIPanel`
  (container), `UISafeArea` (a container whose rect is the parent's minus the insets — Unreal's
  SafeZone; fewer concepts than a flag on every widget).
- **Rich text is `UIText` with runs**, later: a tag parser into runs, `<b>`/`<i>` = a family
  style-key resolve (**TX1** already does it), wrapping *across* runs is the hard part.

### Space — canvas units ARE reference pixels, and the pass is a `CameraView`
- The canvas pass renders through `CameraView{ centre, referenceHeight / 2 }`. **CAM2** — vertical
  half-extent, width follows the target's aspect — is Unity's CanvasScaler with Match = height,
  for free, chosen for the shmup for the same reason.
- The canvas's visible rect is `(referenceHeight × targetAspect) × referenceHeight`; the root
  widget's rect is that; anchors absorb the width. 21:9: top-right stays top-right, a centred menu
  stays centred, a 0..1-anchored background covers.
- **Resize is a projection change first.** Same aspect → nothing in canvas space moves, zero
  layouts. Aspect change → the root rect changes → one re-layout of what depends on it.
- Portrait mobile is the case Match = height gets wrong; a per-canvas match parameter is the
  escape hatch. Named, not built.

### Invalidation — their canvas-cost note, answered
- **Two dirty flags.** `Layout` (my rect changed → my subtree re-lays) and `Content` (my text /
  sprite changed → my cached quads rebuild). A change propagates **up** only through ancestors whose
  size depends on children (layout groups, later) and **stops at the first size-independent
  ancestor** — the usual "layout root".
- **Layout runs at most once per frame**, at the start of the UI tick, against the latest target
  size — a resize storm coalesces to one walk.
- **Submit is per-frame and cheap, and that is why Unity's cost does not transfer.** `Renderer2D` is
  immediate-mode and **F5** records the pass whole every frame anyway: there is no retained mesh to
  rebuild. What IS retained: computed rects and `UIText`'s `TextQuad`s. Per-frame cost is one
  submit per visible quad.
- **The instrument:** `Layouts` and `Text relayouts` per frame as **ST** rows — *0 when idle* is a
  number, not a claim ([[L59]]). A test asserts a same-aspect resize lays out nothing.

### Rendering — one pass per canvas, MV1's named growth point
- Submitted into **the same target as the game world**, `ELoadOp::Load` — its first caller, exactly
  as `ICommandBuffer.h` says. **F5** sorts within the pass, so draw order within the canvas is tree
  order (traversal index → order bits), independent of world layers.
- **Masks are a clip rect as a VERTEX ATTRIBUTE** — **F4d**'s idiom, a value not a pipeline.
  Unity's `Mask` is stencil, which would break F5's one-pipeline batch mid-pass; `RectMask2D` is
  the shape. Vertex 48 → 64 bytes. Nested masks intersect. Sprite-shaped masks named, not built.
- **Which targets get the pass is a design question for U2, not settled here:** the editor viewport's
  FBO in PIE and the backbuffer in `Sandbox.exe` — never over the editor chrome — and the Camera
  Preview (a game view that *"must look like the GAME"*, **MV1**) deliberately NOT. Two shapes:
  the world view's `RenderPassRequest` carries a `bDrawUI` the host sets, or the tenant asks which
  target framed the active world. Renderer must not learn about UI (direction: UI → Renderer).

### Input
- Pointer → canvas space via `ScreenToWorld` with the canvas view — **CAM2**'s one screen→world
  rule, no second one. Hit-test walks the tree top-most first; a point clipped by a mask misses.
- **"The UI captured the pointer" means A WIDGET WAS HIT — never "the pointer is inside the canvas"**,
  because the canvas covers the whole screen. [[L29]] is this exact bug in the editor
  (`WantCaptureMouse` answered ImGui's question, and one of its windows was the game).
- Consumption: the tenant ticks before input mapping resolves (**BO4d** order); the mechanism —
  a published capture flag the mapping subsystem honours, or a UI mapping context pushed only while
  a widget is hit (**IM6**) — is an open question, decided in U3.
- Keyboard/gamepad **focus and navigation** are the hidden cost of `UIButton`. Named, not built;
  gamepad is a WHEN (**IM11**).

### Hosting — the consumers, after the core
- **`UISubsystem`, a GameInstance tenant (GI2)** — owns the persistent canvases: loading, pause,
  the HUD. Outlives every world (**GI1**), reconstructed per PIE cycle (**GI6**) so nothing leaks,
  ticks and submits with zero worlds. **IM2** made the same tier call for the UI *input* context.
- **World-side hosting** — a world subsystem owning a level's own overlay. Trivial once the core
  exists (a canvas is an object anyone can own and submit); built when a level wants one.
- **Loading screen, today:** the tenant shows the loading canvas; `OpenLevel` becomes a REQUEST
  resolved at the next frame's start (**WS8**'s flag-then-resolve shape) so one frame draws the
  cover before the synchronous swap. **Later:** the same canvas stays up across frames with a
  progress binding. The UI does not change; only what is underneath it does.
- **Editor:** `.opaaxui` = the serialized tree (a resource type). A **UI panel** in the
  AnimationLibrary / FontFamily / prefab-panel family: tree on the left, the canvas rendered at
  reference resolution through **MV** (panel-owned FBO), reference frame + safe zone drawn with
  `DrawBounds`, `DrawProperties` on the right. Its own typed undo steps on the panel's stack
  (**PF10**/**UN2** shape). **MR2i**'s route table applies in full. *This is the "Unreal editing"
  note — UMG's designer from parts that exist.*

### MVVM — named, not built first
- The industry converged on MVVM (Noesis by construction, Unreal's UMG Viewmodel, Unity UI
  Toolkit bindings). Its value is data binding, which needs (1) read a property by name — **the
  reflection already does this** — and (2) change notification, which does not exist and which a
  per-frame pull replaces at HUD scale for free.
- So: a gameplay system writes the score into a `UIText` first. **`UIBinding`** — pull a named
  reflected property from any `CReflected` object the game owns into a widget property each frame
  — is the growth point, built the day three HUD elements do that by hand. Noesis's shape without
  its notification machinery. MVC's controller half is already **IM6**: a menu is a context that
  consumes.

## Proposed order — by what becomes visible (theirs to reorder)
| Step | Lands | Visible |
|---|---|---|
| **U1** | `UI/` module: `UIWidget`, `UIRect`, layout walk, invalidation, `UICanvas`; headless tests (anchors under an aspect change; dirty stops at a fixed-size ancestor; same-aspect resize = 0 layouts) | nothing — the tests |
| **U2** | the canvas pass (Load's first caller), `UIImage` Simple + Filled, `UIText` wrap + align with cached quads, the tenant with a hard-coded tree, ST rows | a score and a health bar over `PhysicsTest` in `Sandbox.exe`, on a resized window |
| **U3** | hit-test, `UIButton` states + `OnClick`, pointer capture vs mapping | a pause menu that consumes Jump |
| **U4** | `.opaaxui` + the UI panel + undo + MR2i routes | the HUD authored, not hard-coded |
| **U5** | `UIMask` (clip attribute), `UIImage` Sliced, `UISafeArea` | a stretched panel; a clipped list; a safe-zone frame |
| **U6** | deferred `OpenLevel` + the loading cover | a level swap with the canvas up throughout |

**Later, each with a caller before it is built:** rich text · layout groups (the row of lives) ·
canvas-group alpha (fade a menu) · `UIBinding` · focus/navigation · tweens · sprite-shaped masks ·
world-side hosting · progress across frames (async).

## Open questions (theirs)
- Pointer capture mechanism: a flag the mapping subsystem honours, or a hit-only UI context (U3).
- Which game views get the UI pass, and how the host says so (U2).
- Does the HUD prefab-like reuse exist — a `.opaaxui` nested in another — or is one file one canvas?
  (**PF13**'s answer was "nesting is the resolver's"; ask before U4.)
- `OpenLevel` deferral: a one-frame request is the smallest change; is it the one they want, or
  does the cover wait for async proper?
