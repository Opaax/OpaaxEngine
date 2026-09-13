# Parenting — record (2026-09-13, H1–H5; contract **§HR**, HR1–HR9)

> The block ⑦-C's **K10** stood in for. Plan: `C:\Users\engue\.claude\plans\eager-honking-badger.md`.
> Legacy had one (`Legacy/ECS/Hierarchy.*`, `ParentComponent`): parent-only, world walked, cycles
> refused, drag rows with a "detach" strip. Brought forward in shape; identity is a Guid now and
> prefabs derive guids, which is where all the new work was.

| Step | Commit | What landed |
|---|---|---|
| baseline | `b68a203` | Three test includes stale from their restructure ([[L10]]'s `Engine/Tests` blind spot, theirs). |
| **H1** | `265794b` | `EntityMeta::Parent`; `Compose`/`ToLocal`; `EntityHierarchy`; cascade; map v4 / prefab v3. 13 cases. |
| **H2** | `5a57f5c` | `ComposeChain`, the one walk; every reader reads world; physics + mover write through the verb. 4 cases. |
| **H3** | `aad0612` | Links derive with guids; Fold diffs against the built instance; `parent` in the patch; BuildVariant on derived ids. 9 cases. |
| **H4** | `18cdf21` | `EntityTreeView`; drag-drop; `EntityReparent` + `UndoStack`; Detach; Delete takes the subtree. |
| **H5** | `fc752e4` | The Gun under `PhysicsPlayer` in `PhysicsTest.opaaxmap`. |

**784 → 810 / 8824 → 9029 / 7.** Both hosts boot with no errors; the only new warning is each
existing map's one-time round-trip line on the version digit (**HR9**, stated in the plan).

## Their two calls (asked, both the recommendation)
- **A cross-map drop MOVES the subtree into the parent's map** — the drop is the author saying where
  it goes; it is also "move entity to map" for free (a root onto another map's header).
- **Dogfood = the gun under the player**, not a re-authored turret. There was no gun ENTITY — only
  the component type — so H5 added one to the map that holds the mover-driven player.

## The one defect a test found before it shipped → [[L93]]
`BuildVariant` folds the prefab document's world, which holds the base's RAW guids; a record's patch
is applied over DERIVED entities. A re-parent recorded raw dangled in the flat variant. Fixed by
putting ids and in-base links on derived ones before the fold. Nothing on the level path could have
shown it.

## Decisions worth re-reading
- **The link is identity, not a component** (HR1): the guid-remap and the phantom-override hazard
  both live at the hand-written `EntityData` fields, so that is where the field goes.
- **World is walked, never cached** (HR3): no dirty flag, no order, no stale frame. The cache is a
  growth point with a measured trigger, MP5's shape.
- **Fold's baseline is the built instance** (HR5): it was equivalent to the raw template by accident
  until the first derived field.
- **`BuildPrefab` judges an outside parent only for an UNFOLDED capture**: a document's own entity
  may hang under a nested instance's entity, and the flat set is not there to check against.
- **The scale near-miss in `SyncDynamicTransforms`**: the first draft copied the LOCAL component,
  set world position/rotation, and handed that to `SetWorldTransform` — the local SCALE would have
  been divided by the parent's every step. Build the whole pose in world space first.

## Technique
One throwaway harness in the real editor ([[L81]]): reparent → undo → redo → move the parent →
detach, logging world/local/parent/map per entity and the dirty transitions. It proved the verb,
the step and the dirty gate end to end; the drag GESTURE, the tree rendering and the header drop
are the eye gate.

## Owed — the user's eyes
Drag a row onto another (nested, nothing moves, Ctrl+Z) · onto a map header (root, in that map) ·
delete a parent (children go, Ctrl+Z returns all) · gizmo on a parent moves the children · parent +
child both selected moves once · the prefab panel's tree and its strip while dragging · **open
`Levels/PhysicsTest`, Play, move: the Gun rides the player.** `Save Level` once clears every map's
round-trip warning.

## Named, not built
World-pose cache (HR3's trigger) · sibling order · drop a prefab onto a row to instantiate as its
child · *Create Empty Child* · a world-pose line in the Transform drawer · child count on
`EntityMeta` (HR6's trigger). **Hand-authoring recipe correction:** `json.dumps(sort_keys, indent=4)`
is NOT byte-exact for arbitrary floats (nlohmann's Grisu printed `…563`, Python `…562` for one
double) — splice into the original bytes, or let the editor save.
