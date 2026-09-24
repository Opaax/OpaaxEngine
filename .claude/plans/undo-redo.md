# ⑤ UNDO / REDO — design + record

> **Status: COMPLETE and USER-VERIFIED (2026-09-02).** All four steps' gates passed by hand
> (*"All gates passed"* ×3, *"Ok all gates passed!"*). Zero warnings · **503 / 7308 / 7** ·
> `commands=30`. Contract → `ARCHITECTURE.md` **UN1–UN6** (rewritten; **SEL6** and **MR2b** amended)
> · lesson → [[L73]].
>
> **This file was rewritten on 2026-09-02.** The 2026-09-01 design it described shipped, was run by
> the user, and was rejected. What follows is the design that replaced it. The old one survives only
> as §0, because the reason it failed is the most useful thing in this document.

---

## 0. The version that shipped and was rejected — and why

The first design made the executed `IEditorCommand` the undo entry: `EditorCommandRegistry::Execute`
bracketed every dispatch with a before/after capture of the entities the verb touched, kept the
command alive as the step, and — because a `TPropertyDrawer` writes straight through a `T&` (**I15**)
and announces nothing — covered field edits with a **standing baseline plus a per-frame poll** that
committed a step whenever the values *stopped changing for one frame*.

The user ran it and rejected it in one message:

1. *"Tweaking size from 100 to 150 → undo 149 → undo 148…"* — a slow drag pauses constantly, and every
   micro-pause committed a step and re-baselined.
2. `[MapSerializer] Captured 1 of 1 named entity(ies)` flooding the log — the poll called
   `CaptureEntities` every frame.
3. *"Undo class have many function to record or whatever undo things."* — 17 public members.

**All three were one function**, `EditorUndo::RefreshBaseline`. And the cause of *that* was a
justification I never checked:

- The record claimed an earlier edge-driven version *"silently missed every generic property edit
  while catching the hand-written ones."* **`ImGui::IsAnyItemActive()` is a global query — it cannot
  discriminate.** The real failure was that a gesture recorded nothing unless a command was dispatched
  inside it, and nothing dispatched `EditPropertiesCommand`. A wiring bug, written up as a limitation.
- **The plan, the command's own doc-comment and `ARCHITECTURE.md` UN5 all specified the edge design.**
  Only the implementation had drifted to a poll. Three documents were right and the code was wrong.
- **No engine infers a step boundary from settling.** Unreal brackets at the widget
  (`OnBegin`/`OnEndSliderMovement` → `FScopedTransaction`); Unity groups on the mouse-down event;
  Godot (`MERGE_ENDS` + an 800 ms window) and Lumix (`IEditorCommand::merge`) merge in the stack. All
  four push "what is one step" **outward, to whoever made the edit**.

Three sub-agents ran in parallel to price alternatives (reference-engine research, a whole-world
snapshot design, a widget-lifecycle-seam design). Their value was the **evidence** — the research fact
above, and an independent catch of the `IsAnyItemActive()` misdiagnosis — because the user rejected
all three designs for a fourth of their own.

---

## 1. The design — the user's, stated in three messages

> *"The undo system do not care about a property changing or whatever. The property itself calls undo
> to record its own stuff. ex: Translate a gizmo (begin cache begin position, end cache new position
> then send it to `Undo.Record(GizmoTranslate({entt, positions}))`)"*
>
> *"Undo just record undoable obj thanks to concepts."* · *"Lets make smaller objects."*

**The stack is dumb.** It holds type-erased undoable objects, replays them, and names them for the
Edit menu. It knows nothing about `World`, `MapData`, `EntityOps`, `EditorSelection`, revisions or
ImGui, and its header includes no engine header at all.

**This reverses UN1**, the user's own 2026-09-01 steer that the command instance is the entry. What
that steer bought — a step carrying data an entity snapshot cannot express — is better served now:
*any* type satisfying the concept enters the stack, without being a command.

### The machinery — `Editor/Undo/`, three files

```cpp
// EditorUndoableConcept.h — the exact parallel of EditorCommand
template<typename T, typename ContextType>
concept EditorUndoable = requires(T InStep, const T InConstStep, ContextType& InContext)
{
    { InStep.Undo(InContext) } -> std::same_as<void>;
    { InStep.Redo(InContext) } -> std::same_as<void>;
    { InConstStep.Label() }    -> std::convertible_to<const char*>;
};
```

`IEditorUndoable` erases it with `IEditorCommand`'s `Concept`/`Model<T>` shape, three virtuals instead
of six, and `EditorContext` only **forward-declared** — `Model<T>` instantiates at the `Record<T>` call
site, where it is complete.

`EditorUndo`: **`Record` · `Undo` · `Redo`**, plus `CanUndo`/`CanRedo`/`UndoLabel`/`RedoLabel` for the
Edit menu, plus `Clear`. **8 public members, 3 members** (two vectors and a private `m_bApplying`),
down from 17 and 15.

- **`Label()` is an INSTANCE method.** That is what lets a step name itself from its own data, and it
  is what allowed the gesture API — whose only remaining job was carrying that label — to be deleted
  rather than renamed.
- **Policy is not the stack's.** `UndoCommand`/`RedoCommand` gate on `MapOps::CanEdit`;
  `EditorService::HandleWorldDestroyed` calls `Clear()` when an **Edit**-mode world dies, asked of
  `World::GetMode()` rather than a recorded world id. Same behaviour as before — a PIE cycle destroys
  the Play clone, so history survives Play/Stop.

### The steps — seven small typed objects, four of which serialize nothing

| Step | Payload | Built by |
|------|---------|----------|
| `EntityCreate` | the entity as `MapData` | `EntityOps::Create` |
| `EntityDelete` | the entities as `MapData` | `EntityOps::DestroySelected` |
| `EntityRename` | `Guid` + two `OpaaxString` | `EntityOps::Rename` |
| `EntityTransform` | `{Guid, TransformComponent Before, After}[]` + its name | `ViewportPanel` |
| `ComponentAdd` | `Guid` + type name | `EntityOps::AddComponent` |
| `ComponentRemove` | + the component's `json` | `EntityOps::RemoveComponent` |
| `EntityComponentsEdit` | `Guid` + the **changed** `ComponentData` both sides | `InspectorPanel` |

Create and delete are the same two bodies run in opposite directions; restore re-selects and destroy
clears, which is why the selection needs no payload of its own.

### The two multi-frame edits — the panel holds the step across frames

`Begin` on the rising edge, `End` on the falling edge, one `Record` in between. **No baseline, no
poll, no gesture API.** Both edges already existed in the code for other reasons.

- **Viewport** — `ImGuizmo::IsUsing()`. Closes in `OnPreRender` after `ApplyGizmoDrag`, never at the
  ImGui-pass edge: a delta measured in frame N is applied in N+1 (**SEL3**).
- **Inspector** — `ImGui::IsAnyItemActive()`, the line it already kept for `MarkChanged`. **Global is
  the point:** a resource dragged from the Browser holds `ActiveId` over there, so the bracket opens
  before the drop and closes on it. (The plan wrongly listed that path as uncovered; verified by hand.)
- **The rising edge is read AFTER the drawers ran, and that is correct rather than lucky.** ImGui
  zeroes the drag accumulator on the activation frame and trickles the click and the first move into
  different frames, so `Drag*` has not written; `InputText` has only taken focus; `Checkbox` commits on
  release. The exception is a **click-set** widget — the colour picker's SV square — whose first jump
  sits outside the step. Named, not built; one line to fix if it bites.
- **A step is closed on the falling edge whether or not it recorded.** One left holding entries keeps
  re-reading them, and the next unrelated edit surfaces as a phantom step under the old label. Caught
  while writing S2, before it could ship.

### `EntityComponentsEdit` records VALUES ONLY, and that is load-bearing

`End()` narrows to the components whose *payload* differs. A type on one side and not the other was
added or removed — those have their own steps — and the name is `EntityRename`'s. So committing a name
in the same frame the gesture closes records **nothing** here. The first design's gate 7 ("if one
rename takes two Ctrl+Z, that is this") cannot arise.

The Inspector draws exactly one entity, so it always knows *whose* values changed without knowing
*which field*. Per-component attribution is available — `TDrawerRegistry`'s entry holds
`{Entity, component name}` at the point it pushes the ID scope (**I15**) — and was not needed.

---

## 2. What was deleted

`EditorUndo`: `RefreshBaseline`, `CaptureBaseline`, `BeginRecord`, `EndRecord`, `TrackCreated`, public
`Push`, `BeginGesture`, `EndGesture`, `IsGestureOpen`, `OnWorldDestroyed`, public `IsApplying` —
**11 of 17 methods**; the 6 baseline members, 2 pending members, 3 gesture members and `m_WorldId` —
**12 of 15 members**.

`EntityEdit.h/.cpp` (229 lines) · `TransactedEditorCommand` and `UndoableEditorCommand` ·
`IEditorCommand`'s five undo virtuals, its four `if constexpr` chains and `m_Edit` ·
`EditorCommandRegistry::Execute`'s 20-line bracket · all 7 `UndoLabel()` declarations ·
`EditPropertiesCommand` with its tag and its registration (`commands` 31 → 30) ·
`MapSerializer::CaptureEntities`' Trace line · `EditorSelection::GetRevision`, `m_Revision` and its
three bump sites (added for the baseline, dead with it).

**Added back:** ~20 lines across 6 call sites.

## 3. The cost, stated

**A new mutation verb that forgets to `Record` has no undo, silently.** The dispatch bracket caught
that automatically. The mitigation is **SEL6** — one named choke point, six verbs — and that entry now
says so. It is discipline, not structure, and nothing makes it impossible.

## 4. Gates

**Automated, at every step:** zero warnings, three presets green, **503 / 7308 / 7** unchanged
throughout — `MapRestoreTests` never moved, which is the proof the engine half never depended on the
editor's shape. `commands=30`. An idle 12-second boot logs **zero** `Captured … named entity` lines
and **zero** `Recorded` lines.

**By hand (the user's, all passed):**
- S1 — create · delete 3 · rename · add/remove component, each one Ctrl+Z, values and guids intact.
- S2 — one drag = **one** `Recorded 'Translate' - undo depth N`; rotate/scale a 3-entity selection.
- S3 — **Size 100 → 150 → one Ctrl+Z back to 100** (the reported bug); colour drag; checkbox; a name
  in one Ctrl+Z; Add Component in one Ctrl+Z; **a texture dropped from the Browser in one Ctrl+Z**.

## 5. Open

1. **The block's NUMBER.** `engine-sequence.md` says ⑤; the editor-chrome block that preceded it still
   has no mark. Asked four times across two sessions, never answered — treated as settled at ⑤.
2. The colour-picker click-set frame (§1), named and not built.
