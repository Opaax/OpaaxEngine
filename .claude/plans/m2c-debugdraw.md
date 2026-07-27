# Plan — Editor M2c "DebugDraw + selection outline"

> **Provenance:** third slice of the M2 program — see `.claude/plans/m2-panels.md` for the decomposition,
> the dependency graph and the cross-slice decisions (not repeated here). Base: `d5ec240`, working tree
> **clean** (L17 precondition met). Structure follows the M1/M2a/M2b precedent.
> Final home on approval: `.claude/plans/m2c-debugdraw.md`. Live tracking → `.claude/task/todo.md`.

## Context

M2a landed `EditorSelection`; M2b made it *editable* (Inspector + drawers). Editor.md's **M2 gate is
already closed** — this slice is content, not gate.

Today the selection is visible only as a highlighted row in the Hierarchy. In the **Viewport** — where the
world actually is — nothing marks it. Selecting `QuadRed` and looking at the render tells you nothing.

M2c closes that: an engine-owned `DebugDraw` line queue (Editor.md D10 — *"serves editor overlays **and**
dev builds of `Game.exe`"*, which is why it is engine-owned and not `OpaaxEditorLib`'s), and its first real
consumer, the `ViewportPanel` outlining the selected entity. **This is the only engine-side work left in
M2**, and the only M2 slice expected to move the test count.

---

## 1. What lands

### 1.1 `DebugDraw` (NEW — `Engine/Source/Renderer/DebugDraw.{h,cpp}`)

A per-frame line queue. **Lines only** — no circle, no text, no persistent-duration lifetimes until
something needs one (PL §5 simplicity gate).

```cpp
struct DebugLine { Vector2F Start, End; Vector4F Color; float Thickness; };

// The oriented thin quad covering a segment. PURE geometry, no GPU state — this is the
// unit-testable half, and the reason M2c needs no GL in its tests.
struct DebugQuad { Vector2F Center; Vector2F Size; float RotationRad; };
OPAAX_API DebugQuad ToQuad(const DebugLine& InLine) noexcept;

class OPAAX_API DebugDraw
{
public:
    void DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor, float InThickness = 1.f);
    void DrawBox (const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor, float InThickness = 1.f);

    const TDynArray<DebugLine>& Lines() const noexcept;
    void Clear() noexcept;
    bool IsEmpty() const noexcept;
private:
    TDynArray<DebugLine> m_Lines;
};
```

`DrawBox` pushes exactly 4 lines (centre ± half-extent). `ToQuad` is `{ midpoint, {length, thickness},
atan2(dy,dx) }` — a zero-length segment yields `Size.x == 0` and rotation `0` (IEEE `atan2(0,0)`), i.e.
nothing drawn, no NaN. Methods are **out-of-line in the `.cpp`** so the exe calls the DLL's one
implementation rather than re-emitting inline code over a DLL-owned `std::vector` (I2 discipline; matches
`Renderer2D`). `OPAAX_API` on a non-template class with real compiled members is exactly I6's allowed shape.

**Zero new RHI/shader/vertex-layout surface** — lines are rendered as thin rotated quads through the
*existing* `Renderer2D::DrawQuad(pos, size, color, rotationRad, layer, order)` (`Renderer2D.h:97`).

### 1.2 `ERenderLayer::Debug` (MODIFIED — `Engine/Source/Renderer/RenderLayerList.h`)

One new X-Macro line, inserted **between `Foreground` and `UI`** so the overlay sorts above world geometry
and below UI. `Renderer2D` already sorts the batch by `(Layer, OrderInLayer, texSlot)`, so this needs no
renderer change. Verified safe to insert mid-list: `ERenderLayer` values are **never persisted** — the only
consumers are `MakeSortKey` (8 bits reserved for the layer, `Renderer2DSortKey.h:26`) and
`RenderLayerFromStringID`, which maps **by name**.

### 1.3 `RendererManager` owns the queue (MODIFIED)

`DebugDraw m_DebugDraw;` **by value** (I5 — ownership follows lifetime: a per-frame queue owned by the
thing that drains it, dying with it). Exposed as `DebugDraw& GetDebugDraw() noexcept`.

`Render()` gains a debug pass **after** the world's `Each<DummyComponent>` loop, then drains:

```cpp
void RendererManager::Render(double InAlphaPhysicStep)
{
    RenderFrame(InAlphaPhysicStep);      // existing body, extracted verbatim + the debug pass

    // The debug queue is strictly per-frame and its producers refill it every frame (the editor
    // enqueues in OnPreRender). Draining OUTSIDE RenderFrame's early-outs is what stops a frame we
    // could not render (no RenderSystem / zero-size target) from accumulating lines without bound.
    m_DebugDraw.Clear();
}
```

Extracting the existing body into a private `RenderFrame()` is what makes the drain unconditional; the two
early-`return`s in today's body are precisely the leak path. `Renderer2D& lRenderer` moves up out of the
`if (m_WorldManager)` block so the debug pass can use it.

### 1.4 `IEngine::GetDebugDraw()` (MODIFIED — the seam)

`virtual DebugDraw& GetDebugDraw() = 0;` joins the "Foundation subsystems — exposed directly" block
(`IEngine.h:106`), + a `class DebugDraw;` forward decl. Only two implementers exist (verified:
`Engine`, `NullEngine`), so a new pure virtual is safe.

`Engine::GetDebugDraw()` mirrors `GetWorldManager()` **line for line** — resolve `m_RendererManager` from
`m_Subsystems` first (F3/L6: never re-enter `Startup`), lazy-Startup net, assert, forward. `NullEngine`
returns a function-local static inert queue, exactly like its three siblings (`IEngine.cpp:31-47`).

> *Noted, not done:* this makes a **4th** copy of the ~12-line resolve-from-manager accessor. Deduping it
> into a private `Engine::ResolveSubsystem<T>(T*&)` member template is a genuine simplification, but it
> rewrites three accessors this slice does not otherwise touch — blast radius against a D4
> byte-identical gate, for zero behaviour change (L3). Candidate for a follow-up cleanup commit.

### 1.5 `ViewportPanel` selection outline (MODIFIED — `Editor/Source/Editor/Panels/ViewportPanel.{h,cpp}`)

One private `EnqueueSelectionOutline()`, called at the end of `OnPreRender()`:

```cpp
Entity lSelected = m_Context.Selection.Get();          // value copy — the InspectorPanel.cpp:30 idiom
if (!lSelected.IsValid()) { return; }
const DummyComponent* lComp = lSelected.TryGet<DummyComponent>();
if (lComp == nullptr) { return; }
m_Context.Engine.GetDebugDraw().DrawBox(lComp->Position, lComp->Size + m_OutlinePadding,
                                        m_OutlineColor, m_OutlineThickness);
```

**Zero frame lag, and that is the whole reason it lives in `OnPreRender`.** `EditorApplication::TickFrame`
runs `BeginFrame` (→ `OnPreRender`) → `Engine().Loop()` (world → FBO) → `EndFrame` (→ `Draw`). Enqueuing in
`OnPreRender` means the outline is in the queue *before* the render that consumes it — unlike a panel's
`Draw()`, which is one frame late (M1's viewport-resize handshake, M2b's transform edits).

Padding (`+4` units) keeps the border reading as an outline rather than a rim painted over the quad's own
edge; colour is a bright orange that no `Sandbox` quad uses. Thickness is in world units (1 unit = 1 px at
the FBO's native size) — documented inline, since it does not stay pixel-exact when the panel is scaled.

**L15 — the success branch gets a one-shot `Info` log.** Both early-returns above are silent, and the
"selected an entity with no `DummyComponent`" case would otherwise be indistinguishable from a broken pipe.
One-shot (`m_bOutlineLogged`) so a per-frame path cannot flood — the `m_bImageLogged` precedent at
`ViewportPanel.cpp:82`.

### 1.6 Tests (NEW — `Engine/Tests/Renderer/DebugDrawTests.cpp`)

Added to the **explicit** list in `Engine/Tests/CMakeLists.txt` (that file deliberately does not glob).
Pure data + geometry, no GL, no window — mirroring `Renderer/RenderTargetTests.cpp`'s `StubFramebuffer`
approach, except nothing needs stubbing here. Cases:

1. `DrawLine` appends one entry carrying the given endpoints / colour / thickness; `IsEmpty()` flips.
2. `DrawBox` appends **exactly 4** lines whose endpoints are the rect's corners (closed loop).
3. `Clear()` empties the queue and `IsEmpty()` is true again.
4. `ToQuad` horizontal → centre = midpoint, `Size == {length, thickness}`, rotation ≈ 0.
5. `ToQuad` vertical → rotation ≈ ±π/2, length correct.
6. `ToQuad` 45° diagonal → rotation ≈ π/4, length ≈ √2·d.
7. `ToQuad` degenerate zero-length → `Size.x == 0`, rotation `0`, **no NaN** (pins the `atan2(0,0)` edge).

---

## 2. Verified up front (read, not assumed — L13/L14/L16)

- **Working tree is clean** at `d5ec240` — S2's diff-shape gate (§4) is provable from the start.
- **`ERenderLayer` is not serialized anywhere.** Grepped every live consumer: `Renderer2D.{h,cpp}`,
  `Renderer2DSortKey.h`, `RenderLayer.h`. Nothing writes a layer value to disk or config, so inserting a
  band mid-list cannot break saved data. `MakeSortKey` shifts a `Uint8` into bits 32..39 — a 5th band is
  free.
- **`Entity::TryGet<T>()` is non-const** (`Entity.h:63`) — hence the non-const `Entity` value copy above.
- **`Engine::m_RendererManager` is already cached in `Startup`** (`Engine.cpp:123`), so the accessor's
  resolve path is the one that already works for `GetWorldManager`.
- **CMake needs one edit only.** `Engine/CMakeLists.txt` globs `Source/Renderer/*.cpp` with
  `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so `DebugDraw.{h,cpp}` is picked up free; `Editor`'s glob covers
  the panel. Only `Engine/Tests/CMakeLists.txt` (explicit list) must be touched.
- **No stale-green risk on the accessor change:** `IEngine.h` is included broadly, so S1 forces a genuine
  recompile of the blast area rather than trusting up-to-date `.o`s (L14).

---

## 3. Steps (each builds green, each ends in a commit — L17)

**S1 — Engine: `DebugDraw` + `ERenderLayer::Debug` + `IEngine::GetDebugDraw()` + tests.**
Everything in §1.1–§1.4 and §1.6. Touches **only `Engine/**`**.
*Gate:* 3 presets `OPAAX_BUILD_OK`; test count **up from 85/354/2** (record the exact number);
`Sandbox.exe` byte-identical — which is the real assertion here: a new engine API with no producer must be
inert (empty queue → one `Clear()` on an empty vector per frame, no new log line).
**→ commit before S2** (S2's gate is a property of its diff).

**S2 — Editor: the selection outline (first consumer).**
Everything in §1.5. **This step's diff must touch only `Editor/**`** — the checkable form of "the engine
API landed complete in S1 and needed no follow-up," the same discipline M2a/S3 and M2b/S2 used for
`Sandbox/Editor/**`.
*Gate:* click `QuadRed` in the Hierarchy → an orange box appears around the red quad **in the Viewport, the
same frame**; click `QuadGreen` → the outline moves; drag Position in the Inspector → the outline tracks the
quad; the one-shot `Info` line confirms the enqueue.
**→ commit.**

---

## 4. Verification

1. **3 presets** grep `OPAAX_BUILD_OK` (`debug-editor` / `release` / `release-editor`) — never the exit
   code (L8).
2. **Tests:** `./build.bat test`. Baseline 85 / 354 / 2; S1 raises it. Record the exact delta — this is the
   only M2 slice that should move it at all.
3. **`Sandbox.exe` byte-identical (D4)** at *both* steps: 3 quads, 1280x720, 0 err/warn, no new log lines.
   Smoke pattern from `CLAUDE.local.md`: `./Sandbox.exe > log 2>&1 & sleep N; taskkill //IM Sandbox.exe`,
   then grep the log (graceful, no `//F` — that skips clean teardown).
4. **`SandboxEditor.exe`** — the §3 observable gate per step, pointing at specific UI **and** a log line,
   never "no errors" alone (L12/L15).
5. **S2's diff touches only `Editor/**`** — changed + untracked paths filtered for anything outside it must
   come back empty.
6. **Graceful window close** → clean reverse teardown, 0 err/warn. Nothing new to order: `DebugDraw` is a
   by-value member of `RendererManager` and dies with it.

---

## 5. Forks (decided here, with reasons)

- **`DebugDraw` is engine-owned, consumed by `RendererManager`** — Editor.md D10 says it serves dev builds
  of `Game.exe`, which never links `OpaaxEditorLib`, so editor ownership is ruled out outright.
- **Owned by `RendererManager`, not by `Engine`.** The owner is the thing that *drains* it. Under
  `Engine` ownership a failed/absent renderer leaves the queue with no consumer and unbounded growth; under
  `RendererManager` the queue's lifetime is the renderer's (I5) and §1.3's unconditional drain closes the
  last accumulation path.
- **By value, not `UniquePtr<DebugDraw>`.** The `UniquePtr<RenderSystem>` next to it exists to keep an
  incomplete pImpl type out of the header; `DebugDraw` is a small owned queue with nothing to hide. Cost:
  `RendererManager.h` gains `MathTypes.h` (glm vectors) — already ubiquitous in this codebase (every World
  component, all of `Renderer/`). KISS wins over a heap indirection and a null check.
- **Lines only** — `DrawLine`/`DrawBox`. No circle, no `DrawText`, no duration/persistent lines until a
  caller needs one (PL §5 / L3: don't build the expensive version unseen).
- **A new `ERenderLayer::Debug` band, not `ERenderLayer::UI`.** Reusing `UI` would make the overlay
  inseparable from real UI draws the moment a 2D UI pass exists; a band costs one line in an X-Macro list
  built for exactly this.
- **Enqueued in `ViewportPanel::OnPreRender`, not `Draw`** — §1.5. This is the difference between zero lag
  and one frame of lag, and it is the hook `IEditorPanel.h:12` documents for precisely this purpose.
- **The outline reads `DummyComponent`**, the only component carrying position/size and the only one the
  renderer draws (`RendererManager.cpp:129`) — the same call the M2b Inspector already edits. A real
  `TransformComponent` is M3's `ComponentRegistry` v2 work; introducing it here would force a render-path
  change inside a panels milestone (overview §3.1, L3).

---

## 6. Critical files

- **New:** `Engine/Source/Renderer/DebugDraw.{h,cpp}`, `Engine/Tests/Renderer/DebugDrawTests.cpp`.
- **Modified (engine, S1):** `Engine/Source/Renderer/RenderLayerList.h`,
  `Engine/Source/Engine/Subsystems/Renderer/RendererManager.{h,cpp}`,
  `Engine/Source/Application/Services/IEngine.{h,cpp}`, `Engine/Source/Engine/Engine.{h,cpp}`,
  `Engine/Tests/CMakeLists.txt`.
- **Modified (editor, S2):** `Editor/Source/Editor/Panels/ViewportPanel.{h,cpp}` — **only**.
- **Sandbox: none.** Engine `CMakeLists.txt`: none (glob). Editor `CMakeLists.txt`: none (glob).
