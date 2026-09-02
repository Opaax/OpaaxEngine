# ⑥ RENDER GAPS — plan + record

> Block ⑥ of `.claude/plans/engine-sequence.md`. Four items in the sequence doc's own order:
> **global pass sort** → sprite sheets + UV authoring → animation → text rendering → multi-view.
> Each gets its own step, its own gate and its own commit.

---

## S1 — GLOBAL PASS SORT ✅ landed 2026-09-02

**The defect, stated by the code itself** (`Renderer2D.cpp`, old line 247): *"Orders the CURRENT
batch only."* `Renderer2D` sorted by `(Layer, OrderInLayer, texSlot)` **inside `Flush()`** — after
the batch had already been cut, and a batch was cut mid-record when the vertex buffer filled
(`MAX_QUADS = 1000`) or the samplers ran out (16). So past either threshold the painter's algorithm
held only *within* a batch: batch 0 drew a `UI` quad, batch 1 then drew a `Background` quad over it.
The 1000-bullet shmup. ④ shipped the `Draw Calls` counter specifically to make it visible first.

**The fix:** record the pass whole → sort once → cut into batches. Contract → **F5** (+ **ST7**
amended, **F4d** untouched).

### What shipped

| | |
|---|---|
| `Renderer/Renderer2DBatchPlan.h/.cpp` | `PlanQuadBatches(keys, texIds, limits, OutPlan)` — free, pure, `OPAAX_API`. Stable-sorts the pass, then walks it assigning sampler slots and cutting batches (quad room checked before samplers). Limits below 1 quad / 2 slots are read as 1 / 2, so no input can hang it. |
| `Renderer2D` | Records `PassVertices` / `PassKeys` / `PassTexIds` / `PassTextures`; `EmitPass` walks the plan, gathers into `UploadBuffer` writing `TexIndex` from the placement, flushes per batch. `Flush(quads, slots)` only uploads-binds-draws. |
| `RenderLimits` | Live at last — buffers sized at `Init` from `MaxQuads`, slots capped at `SHADER_TEXTURE_SLOTS = 16`, out-of-range clamped **loudly**. The "deferred" NOTE is gone. |
| `RendererConfigData` | `MaxQuadsPerBatch` (1–65536) + `MaxTextureSlots` (2–16), both `NeedRestart`; `RendererManager::Startup` fills `lDesc.Limits`. |
| Deleted | `EnsureBatchRoom`, `GetTextureSlot`, `StartBatch`, the fixed `VertexBuffer`/`SortKeys`/`SortedBuffer`/`TextureSlots` arrays, and the `DrawSprite` "ORDER MATTERS" trap with them. |
| Renamed | `BeginScene`/`End` → `BeginPass`/`EndPass` — **F2** retired "scene" long ago and this was its last user. |

### Gates

- **`BatchPlanTests.cpp`, 8 cases.** The regression gate is case 3: `[UI, Bg, UI, Bg]` with room for
  two quads must emit both `Bg` in batch 0 and both `UI` in batch 1 — **an ordering the old code
  could not produce**. **511 / 7346 / 7** (from 503 / 7308 / 7), zero warnings, all three exes link.
- **Smoke, `MaxQuadsPerBatch: 2`:** `Renderer2D::Init(device) — 2 quads and 16 texture slots per
  batch`, then `Pass split into 3 batches for 6 quads`. 163 log lines, zero errors, zero warnings,
  clean teardown. **The multi-batch path had never run before this.**
- **Owed to the user's eyes** (a smoke run has no eyes): at `MaxQuadsPerBatch: 2` the viewport must
  look **identical** to `1000` — ③b's grid behind, sprites in authored order, selection outline on
  top — while Stats' `Draw Calls` reads ~4 and its amber tooltip now says *cost, not drawing error*.

### Follow-up from the user's own gate run (same day, `3167b0b` · `594468d`)

They ran it, saw `Draw Calls` go 2 → 3 at a 2-quad limit, and reported the number was **amber**.
- **The amber row is deleted.** It was ④'s instrument for making the ⑥ bug visible; F5 retired the
  bug, so the colour was warning about a limit the author configured on purpose. → [[L74]], **ST7**.
- **`MaxQuadsPerBatch` defaults to 10000, not 1000** (1.9 MB at 192 bytes a quad). 1000 was a
  bring-up number that splits every frame of a bullet-heavy scene. **An explicit key still wins** —
  a project that has saved the value keeps it.
- **`PropertyMeta::SetTooltip` + `IEditorWidgets::HelpMarker`** — their question *"why do we give
  limits like this?"* was a missing tooltip, so both limits now carry the answer in the Config
  panel. Design notes in **I15** and **MR2h**; the marker is its own `(?)` item because MR2h forbids
  decorating the previous one, which turned out to be the better shape anyway.

### Notes for whoever reads this next

- **The texture in the sort key is a PASS id now, not a batch slot** — a slot cannot exist before
  the batch does. `MakeSortKey`'s bit layout is untouched, so `SortKeyTests` is untouched too. Above
  255 distinct textures in one pass, ids collide in the key's low byte: grouping degrades, ordering
  does not (the layer and order fields are unaffected).
- **Widening that field was considered and refused** — 256 textures in one pass is already ≥17
  batches, so the tie-break has stopped mattering by then, and the change would rewrite a tested bit
  layout for nothing observable.
- **Sorting per PASS, not per frame,** is the honest unit: each pass has its own view-projection and
  target, so ordering across two of them is meaningless. This is what multi-view (below) needs.

---

## Still to come in ⑥ (not planned, not started)

- **Sprite sheets + UV authoring, then animation.** `DrawSprite` already takes UVs and nothing
  authors them. A sheet is its own resource type + a component + a world subsystem advancing frames
  (**D7**'s data-plus-subsystem shape, never a polymorphic component).
- **Text rendering.** The DebugDraw/HUD consumer, and what unblocks a `Game.exe` stats overlay.
  `Old/milestone/M5_Text_Rendering.md` is salvage.
- **Multi-view.** Nearly free now: one `BeginPass`/`EndPass` bracket per view, each sorted whole.
