# ⑦-C — Prefabs

**CODE-COMPLETE 2026-09-10.** 37 commits `e3cc05b` → `e288466` over two days. P1–P6, P8 V1–V4 and
P5b are user-verified (*"Its working!"* · *"The reconcile works fine"* · *"eye gate passed"* ·
*"ok, works"*); **P7's eye gate is OPEN** — mechanism verified by harness, not yet by their hands.
**723 / 8423 / 7 → 781 / 8781 / 7** (the suite wobbles ±2 assertions between runs, pre-existing).
Durable: **§PF** (PF1–PF13), **MP12**, **GIZ5**, **UN2**, **CAM2**, **MV1** as amended.
Lessons: [[L87]]–[[L91]].

---

## What it is

A `.opaaxprefab` is a map's entities in a file (one writer, **PF1**). A map places one by storing a
LINK and the DELTAS — `{ prefab, instanceId, overrides }` — never the entities (**PF3**); an
instance's guids are DERIVED, `Guid::Derive(instance, template)`, so nothing is stored or remapped
(**PF2**). Saving a prefab is an EVENT bracketed around the write, and every placement in the level
follows, keeping its own overrides (**PF8**). A prefab is edited in a WORLD OF ITS OWN, in a panel
that is a real viewport with its own camera, selection and history (**PF9**/**PF10**/**PF12**). A
component author says whether a reference loads NOW or LATER by its type alone, and the map that
mounted the entity holds what it must (**PF11**). A prefab may place prefabs; the resolver hands
every consumer the prefab FLATTENED, and a variant is one record in a file (**PF13**).

---

## The slices

| | |
|---|---|
| **P1** `e3cc05b`, `bcd07cd`, `f1ecdd3` | The format, the resource, derived guids, `BuildInstance`; the turret. |
| **P2** `ca467e2` | Create Prefab from Selection is a SWAP, not an export. |
| **P3** `c8e21ec`, `77be913`, `c8877a0` | Map v3: link + merge-patch deltas; fold/expand; Revert is "be this again". |
| **their fixes** `66c9220`, `3958236` | A removal is a null patch (L87); float from file == float from capture (L89). |
| **P4** `aec6caa`, `51942f3` | `ResourceOps::AboutToSave/SavedToDisk`; the reconciler. The bracket goes around the WRITE (L88). |
| **P5a** `d45a710` | `THardResourcePath<T>` — an alias, zero churn; hard fields discovered by type. |
| **P6** `4670028` | The prefab document owns a world; Save announces. |
| **P8 V1** `1a105b0` | `TransformEntities(World&, …)` — the mutation verb names its world. |
| **P8 V2** `7babba0`, `5af0fcd`, `7890260` | A debug primitive names its world; gestures shared under `Editor/Viewport/`; the panel pans, zooms, picks. |
| **P8 V3** `c0fc0c7`, `0a37c55`, `2d5a10b`, `f322f5a` | `GizmoDrag` per surface; a panel DECLARES its Undo/Redo; the document owns its stack (L91). |
| **P8 V4** `c7b13af`, `41b2635` | Steps name world AND selection; property edits and Delete land on the document's stack. |
| **fix** `b989953` | A prefab that REMOVED a piece takes it from every placement — on save and on revert. |
| **P5b** `70a029c`, `7c3f114`, `9beb376` | The acquire, at the LEVEL: `MountedMap` holds erased refs. The Sandbox gun names its bullet. |
| **P7a** `f231ec7` | Prefab v2 holds records; `Flatten`; the resolver flattens, owns, and refuses cycles; `Places`. |
| **P7b** `462e7d2` | Instantiate through the resolver; affected follows nesting; the document expands/folds. |
| **P7c** `e288466` | Drop a prefab on the preview to nest it (self/transitive refusal); Save As Variant… |

---

## THEIR DESIGN CALLS, all load-bearing

- **Multi-entity prefabs from P1**, with the whole instance selected as the stand-in for parenting
  (**K10**) — the gizmo already transforms a list.
- **`THardResourcePath` as an alias** — no existing declaration changed.
- **Overrides as instance RECORDS in the map** (the wide option), which is exactly what P7 promoted
  to a file: a variant is a record with no siblings.
- ***"Save call event… This can apply for all things too right?"*** — why **PF8** is a generic
  `ResourceOps` bracket and not a prefab hook.
- ***"Viewport-ify it, undo included"*** — a gizmo drag with no undo is worse than no gizmo; V3
  shipped both together. The shared W/E/R mode across both viewports is theirs (Unity/Godot).
- **Ctrl+Z doing nothing in the panel was the swallow WORKING** — the real gap was no prefab undo.

## THE FIVE DEFECTS THEIR EYES FOUND — each a design hole, not a typo

1. *"deleting one piece of the turret…"* — the format could say what an instance CHANGED and not
   what it REMOVED ([[L87]]). Its other half surfaced a day later as *"Delete from panel prefab is
   not reconcile"*: the reconciler and the revert both left orphans, fixed as one verb over.
2. Drag-drop *"do not work"* — never built; deferred in P1 without saying so.
3. Phantom overrides on a fresh placement — float↔double through json ([[L89]]).
4. *"Save do not reconcile in edit world"* — the bracket was around the reload ([[L88]]); every
   mechanism worked and the feature did nothing.
5. Ctrl+Z undoing the LEVEL behind a focused prefab panel — **PF10**, retired by V4.

## The three altitude decisions worth re-reading

- **P5b — the acquire lives at the LEVEL, not in the loader.** The prefab resource is released the
  moment an instance is instantiated (**PF4**), so a hold hung off its payload would not outlive
  the gun ENTITY. The map that mounted the entity does. `ResourceManager`/`LoadContext` untouched.
- **P8 — the VIEW is the panel's, the rest is the DOCUMENT's.** An undo step replays against a
  selection, and a step can reach a document, never a panel.
- **P7 — nesting is a property of the RESOLVER.** Every consumer already asked one call for "the
  prefab's entities"; flattening there cost them nothing. Only the document reads the raw structure.

## Technique

Eleven throwaway harnesses ([[L81]]) across the block: frame-driven code in `OnPreRender`, temp
files under `_Harness*`, numbers logged, removed, the shipped build re-smoked. With ImGui
viewports a floating panel is its own OS window, so a harness that shows it leaves a process
`taskkill //IM` cannot close — kill by PID, confirm exit, `cmp` the layout before reading a log.

## Named, not built

Create Variant from a placement in the LEVEL (promote its record, relink) · Revert to Prefab
inside the prefab panel, and a nested instance's own revert · hard-ref holds for runtime spawns
and editor placements after mount · the prefab preview's snap grid · the Edit menu's Undo label
following the focused panel · add/remove-component in the panel · a per-surface W/E/R mode.
