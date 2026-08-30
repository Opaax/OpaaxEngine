# ③b VIEWPORT TOOLBAR — the record (landed 2026-08-30)

> Contract: ARCHITECTURE.md **GIZ8–GIZ10**, amending **GIZ3** (the choke point takes a
> `TransformDelta`), **GIZ5** (`ReseatAt` takes and remembers a rotation) and **GIZ7** (the strip
> stops being a growth point). Lessons **L56**/**L57**.
> Commits: `6fa9c5c` `d69367d` `66ed731` `7449b31` `1cb66cd`.
> Machine gates: 3 presets, **446 / 7067 / 7**, editor boots in 165 lines.
> Eye gates: S1 *"all working"*, S2 *"all good now"* after two fixes. **S3's grid-matched snapping is
> NOT separately ticked** — see below.

---

## Why it exists

③ shipped snapping hard-wired to Ctrl at fixed steps, the pivot always the bounds centre and the
space always `ImGuizmo::LOCAL`. I filed a tool strip as a "named growth point"; the user's correction
was *"yes no. I mean i want a really task for viewport toolbar where we can choose snap etc..."*.

**The requirement that shaped it was theirs too:** *"should be easy to add thing in it too!"* — which
is why this is a REGISTRY (**GIZ8**) and not a row of ImGui calls. Adding a tool, from the editor or a
game module, is one call:

```cpp
Extensions.ViewportTools().Add(OPAAX_ID("MyTool"), [](EditorContext& InContext) { /* widgets */ });
```

## What shipped

Mode buttons (a third front-end onto the existing command tags) · a snap toggle with an editable step
for the active mode, Ctrl INVERTING it rather than setting it · a three-state pivot · a space toggle
that disables itself in Scale mode and says why · a snap grid on the Background band.

## The three defects, and what each taught

**1. The pan wrapped inside the toolbar** (`d69367d`, [[L56]]). `MeasureCameraGesture` found its rect
with `GetItemRectMin/Max`, which name the LAST SUBMITTED ITEM — correct only while the image was
last. Drawing a toolbar before it silently retargeted them. **A second defect shared the cause and
would never have been reported:** the zoom anchor read the same call, so zoom-at-cursor had been
anchoring to the toolbar's origin. Fixed by PASSING the rect, which closes the class rather than the
instance — and S3 added another overlay two commits later.

**2. Scale drifted on a multi-selection** (`1cb66cd`, [[L57]]). A scale delta is `R·S·R⁻¹` in the
GIZMO's frame; I conjugated by each ENTITY's rotation, which cancels only for the primary. Everyone
else got a non-diagonal matrix whose `atan2` is a spurious turn. **Exactly right for one entity,
which is why it passed its tests and its first eye gate.** The fix moved the conjugation out of the
loop entirely and turned the choke point's argument into a `TransformDelta`.

**3. Snapping ignored the grid it drew** (`7449b31`, user's call). Zoomed out the grid coarsens by
decades while the authored step stays put, so a drag landed between two visible lines. Now the step
IS the drawn spacing while the grid is shown.

## The user's design calls

- **Individual Origins** — they drew it: Center/Origin are one shared point that N entities orbit;
  Individual is each entity turning about ITSELF. A different KIND of answer, not a third place to put
  the same point. Implemented as an absence (skip the position multiply).
- **The toolbar is a task, not a footnote** — the correction that started this block.
- **Snap should match what you see** — which is what makes the grid load-bearing rather than decorative.

## Still owed

- **S3's eye gate was never ticked one-by-one.** The user saw the grid and reported the snap mismatch,
  which is fixed — but the fix, the decade step-up and the axis colours have not been confirmed. **Do
  not record S3 as user-verified.**
- Cross-session persistence (needs a `Config_Editor`, the first non-DLL config).
- A game module actually adding a tool — the route is sealed like the rest, but ② deleted the only
  game-module panel, so nothing dogfoods it. Same open gap M2's panel route has.
- Snap-to-vertex / snap-to-entity · rotate/scale for a multi-selection with MIXED rotations
  (**GIZ9** states what it approximates) · `DebugDraw::DrawCircle`.
