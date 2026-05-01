# Todo — Transform rotation + hierarchical parenting

> Source plan: `C:\Users\engue\.claude\plans\fizzy-swinging-clarke.md`
> User scope decisions: UUID-based parent persistence, cascade-destroy default `true`, `SliderAngle` (degrees UI / radians storage).

## Status legend
- [ ] pending
- [~] in progress
- [x] done

> Implementation pass complete (2026-04-30). User builds locally — see `Review` at the bottom for the build outcome of the first compile attempt and the fixes applied.

---

## 1. Renderer rotation (engine core)

- [x] Add `Rotation` parameter (default `0.f`) to `Renderer2D::DrawQuad` / `Renderer2D::DrawSprite` overloads in `Engine/Source/Renderer/Renderer2D.h`.
- [x] Implement rotated corner math in `Engine/Source/Renderer/Renderer2D.cpp`. Axis-aligned fast path kept when `Rotation == 0.f`. Helper `RotateOffset` in anonymous namespace.
- [x] `Engine/Source/Core/World/WorldRenderSystem.cpp` — calls `Hierarchy::GetWorldTransform` and forwards world Pos/Scale/Rotation to `DrawSprite`.

## 2. Hierarchy data + lifecycle

- [x] `Engine/Source/ECS/Components/UuidComponent.{h,cpp}` — uint64 + `GenerateUuid()` via thread-local `mt19937_64`. JSON `"uuid"` stringified.
- [x] `Engine/Source/ECS/Components/ParentComponent.{h,cpp}` — runtime `EntityID Parent`. Component-level Serialize is intentionally empty; serializer writes `parent_uuid` at the entity level.
- [x] `Engine/Source/Core/World/World.{h,cpp}`:
  - [x] Auto-emplace `UuidComponent` in `CreateEntity`.
  - [x] `DestroyEntity(EntityID, bool bDestroyChildren = true)` recurses or re-parents children to root.
  - [x] `FindByUuid(uint64) -> EntityID`.
- [x] `Engine/Source/ECS/Hierarchy.{h,cpp}`:
  - [x] `WorldTransform` POD.
  - [x] `bool SetParent(World&, Child, NewParent)` with self/cycle rejection (uses `IsDescendantOf`).
  - [x] `void ClearParent(World&, Child)`.
  - [x] `WorldTransform GetWorldTransform(const World&, EntityID)` — collects chain root-up, composes leaf-down with TRS-style position rotation/scale.

## 3. Scene serialization

- [x] `Engine/Source/Scene/SceneSerializer.cpp` save: emits `version: 2`, `uuid` per entity, `parent_uuid` when a `ParentComponent` exists.
- [x] Load: two-pass — entities created and restored, then `parent_uuid` resolved via `World::FindByUuid` + `Hierarchy::SetParent`. Unresolved links logged + skipped.
- [x] Backward-compat: missing `uuid` → fresh ID kept; missing `parent_uuid` → root. Old saves load flat.

## 4. Editor — Inspector

- [x] `TransformDrawer.cpp` — `ImGui::SliderAngle` for rotation (degrees UI, radians storage).
- [x] Parent picker `BeginCombo` listing `<None>` + every entity except self/descendants. `Hierarchy::SetParent` invoked on selection; rejected re-parents trigger a "CycleRejected" popup.
- [x] Read-only "World" foldout shows compounded `Hierarchy::GetWorldTransform` values (rotation displayed in degrees).
- [ ] (Skipped) UuidDrawer — not strictly needed; Hierarchy panel/Inspector show stable identity sufficient for now.

## 5. Editor — Outliner (HierarchyPanel)

- [x] Recursive tree via `ImGui::TreeNodeEx` + per-frame `UnorderedMap<EntityID, TDynArray<EntityID>>` index.
- [x] `BeginDragDropSource` / `BeginDragDropTarget` on each row, payload `"OPAAX_ENTITY"` carrying `EntityID`. Reparent via `Hierarchy::SetParent`.
- [x] Bottom drop-zone (`Dummy` + `BeginDragDropTarget`) detaches to root via `Hierarchy::ClearParent`.
- [x] Right-click delete now defers destroy until after the iteration finishes; cascades children by default.

## 6. Documentation / housekeeping

- [ ] Update `code/code-base.md` after manual verification (project structure note for new components/helpers).
- [ ] Update `CLAUDE.local.md` "CURRENT MILESTONE" section.
- [x] This todo.md kept current.

## 7. Verification (manual — user is building locally)

- [x] Editor build clean (debug-editor preset). *(Verified 2026-04-30: incremental cmake build, OpaaxEngine.dll + Game.exe produced, no errors.)*
- [x] Sprite rotation, drag-drop reparenting, cycle rejection, cascade destroy, save/restart/load with UUIDs, and legacy-scene fallback — confirmed working by user during the M1 editor flow (2026-04-30).

---

## Review

### Implementation summary (2026-04-30)

**Files added**
- `Engine/Source/ECS/Components/UuidComponent.{h,cpp}`
- `Engine/Source/ECS/Components/ParentComponent.{h,cpp}`
- `Engine/Source/ECS/Hierarchy.{h,cpp}`

**Files modified**
- `Engine/Source/Renderer/Renderer2D.{h,cpp}` — rotation parameter on every `DrawQuad`/`DrawSprite` overload (default `0.f` so existing callers compile unchanged); CPU rotation of the four corner offsets when non-zero.
- `Engine/Source/Core/World/World.{h,cpp}` — auto-UUID stamp on `CreateEntity`; `DestroyEntity(EntityID, bool bDestroyChildren = true)` cascades by default; `FindByUuid` linear scan.
- `Engine/Source/Core/World/WorldRenderSystem.cpp` — uses `Hierarchy::GetWorldTransform` and forwards rotation to `Renderer2D::DrawSprite`.
- `Engine/Source/Scene/SceneSerializer.cpp` — writes/reads `uuid` + optional `parent_uuid`; two-pass deserialize; tolerates older files (treated as flat hierarchy with fresh UUIDs).
- `Engine/Source/Editor/Inspector/Drawers/TransformDrawer.{h,cpp}` — `SliderAngle` rotation; parent picker combobox; read-only "World" foldout.
- `Engine/Source/Editor/Panels/HierarchyPanel.cpp` — recursive tree, drag-drop reparent, root drop-zone, deferred-destroy with cascade.

**Build status (first attempt, then user took over)**
- Initial build flagged a single error in `ParentComponent.cpp`: `Uint32` not visible in scope. Fixed by removing the now-unused cast (the `Serialize()` body returns an empty JSON object — parent linkage lives at the scene level).
- Defensively added `#include <unordered_map>` and `#include <limits>` to `HierarchyPanel.cpp` and `UuidComponent.cpp` respectively, since `OpaaxTypes.h` does not pull them in transitively.
- All other modified/new TUs compiled in the first pass without errors. Linker did not run (build aborted on the ParentComponent error).
- User is performing the post-fix build manually.

**Decisions captured during implementation**
- ZOrder is summed across the chain (parent + child). Simple, designer-readable; revisit if scene depth increases.
- `IsDescendantOf` and `GetWorldTransform` walk parent chain with a `MAX_HIERARCHY_DEPTH = 256` guard so a corrupted save can't cause an infinite loop.
- HierarchyPanel intentionally collects a single delete request per frame and processes it after the tree walk to avoid invalidating views/registry mid-iteration. The previous code destroyed during iteration and used a `break` to bail out, which still left the registry in an inconsistent state for the rest of the frame.
- Parent picker rejects re-parents that would create a cycle; the drag-drop path logs a TRACE and silently no-ops on rejection (no popup) — matches typical outliner UX.

**Out of scope (deferred)**
- World-transform caching with a dirty flag.
- ImGuizmo rotate/translate/scale gizmo in the viewport.
- "Save Scene As" menu.
- Multi-select reparenting.
- A dedicated UuidDrawer in the inspector.

### Verification (pending — user-driven)

After the manual build succeeds, run through the seven checks in section 7. If any fail, capture the cause in `tasks/lessons.md` and revisit the relevant section here.
