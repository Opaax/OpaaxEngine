# ③ GIZMO — the record (landed 2026-08-28, user-verified)

> Contract: ARCHITECTURE.md **GIZ1–GIZ7**, plus amendments to **CAM2** (three questions, not two),
> **CAM4** (the pan wraps) and **I17** (`Scale` arrived). Lessons **L54**/**L55**.
> Commits: `d076b48` `3dc2d9b` `9277fc0` `fd064b8` `8744775`.
> Machine gates: 3 presets, **434 / 7024 / 7**, editor boots in 161 lines.
> Eye gate: *"Eye gate look goood. Multi select all transform is about pivot point etc..."*

---

## What shipped

A viewport transform gizmo — translate, rotate, scale — driven by **ImGuizmo**, writing through the
one mutation choke point, with **Ctrl snapping** and an **infinite drag** that wraps the cursor at the
viewport edge. `TransformComponent.Scale` arrived with it, together with all three of its readers.

## The two decisions that shaped it, both the user's

**1. Not a screen-space ImGui gizmo — then ImGuizmo after all.** I first proposed drawing on an ImGui
draw list. The user pushed back (*"mmmm not sure about that. Rely on other engines"*), and the survey
inverted my answer: Unreal, Unity, Godot-3D, Godot-2D and ImGuizmo **all size a gizmo from the
screen** and differ only on where geometry is authored — a split that tracks occlusion, i.e. 3D vs 2D,
not taste. That produced the world-space `DebugDraw` version (`d076b48`). They then asked *"Cant we
use imguizmo?"*; I priced both honestly and recommended hand-rolled; **they chose ImGuizmo**, and the
hand-rolled `GizmoHandles` was deleted the same day. **Both readings were defensible — the call was
theirs, and it bought rotate + scale + snapping + bounds in one step.**

**2. The infinite drag.** Asked for during planning, built in `fd064b8`. See **GIZ6** — the whole
content of it is that a *delta* reader and an *absolute* reader need opposite treatment.

## What survived the switch

`EditorGizmo` as session state, the measure-then-apply handshake (**SEL3**/**MP7**), and one verb at
the choke point (**SEL6**). **None of those were about handles**, which is why they outlived the
implementation they were written for.

`EntityOps::TransformSelected(ctx, Matrix44F)` — one verb, one matrix, all three modes. That is what
③ owed ⑤, and it is now true rather than aspirational.

## The defect worth remembering

**ImGuizmo's `deltaMatrix` means a different thing per mode** (**GIZ4**, [[L54]]): incremental for
translate, incremental-and-pivot-conjugated for rotate, but a **cumulative origin-centred scale** for
scale. Applied uniformly it multiplied an entity's position about the world origin — **exactly nil at
(0,0)**, so it looked correct there and only there. The user found it in one sentence whose
*condition* named the cause: *"Scale do something weird when not on 0-0"*.

The fix removed code: the delta is `M * inverse(M last frame)` off the matrix we already own,
per-frame by construction and pivot-conjugated for free.

**It also closed the M2a gap** ([[L55]]). "Tests cannot reach editor code" was true of code that links
`OpaaxEditorLib` and false of a header-only value type; the fix was one include line and no link.
`Engine/Tests/Editor/EditorGizmoTests.cpp` is the first test to reach editor code.

## Named, not built

A **viewport tool strip** — snap settings, pivot-vs-centre, local-vs-world (**the user's own next
step**, 2026-08-28). All three are single expressions with one caller today: snapping is Ctrl-held at
10 units / 15° / 0.1, the pivot is always the selection's bounds centre, and the mode is always
`ImGuizmo::LOCAL`. The strip is UI over values that already exist.

Also unbuilt: a rotate/scale gizmo for a multi-selection with mixed rotations · `DebugDraw::DrawCircle`
(the ring is ImGuizmo's, so that header's *"waits for a caller"* still does) · `DummyComponent`'s
retirement and a per-sprite local `Offset` — both carried in from ②, both **map migrations rather than
gizmo work**, both still owed.

## Consequence to expect, not to fix

Every `.opaaxmap` warns **once** on open that it re-serializes differently, because the writer now
emits `Scale` and the files do not (`first difference at byte 880`). That is **MP6** working, and it
clears on the first Save. **Adding a field to a component will always do this.**
