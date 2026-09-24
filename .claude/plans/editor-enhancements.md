# ② EDITOR ENHANCEMENTS — picking, transform, icons

> **✅ LANDED 2026-08-27, user-verified.** The plan below is the one that was approved and executed.
> Durable outcome: ARCHITECTURE.md **I17** + **SEL1–SEL8** + **MP6**'s ordering clause; lessons
> **L52**/**L53**. Commits `dfcabaf` `b08315f` `45ae230` `3b98b87` `2ad3668` `658fb32` `49d561f`
> `47ef129`. Final gates: 3 presets green, **418 cases / 6918 assertions**, both hosts smoke clean,
> both mounted maps round-trip stable.
>
> **What the plan did not predict, all of it from the user's interactive passes:**
> - The **map migration had to match the writer's ENTITY ORDER**, not just its formatting (→ **MP6**).
> - Three interactive bugs, none reachable by a smoke run, all of them a borrowed ImGui predicate
>   answering ImGui's question rather than mine (→ **SEL8**, [[L53]]).
> - Four upgrades asked for after step 4 and delivered in the same block: **rename**, **remove
>   component**, **unique default names**, and **shortcuts scoped to the selection rather than to the
>   viewport** (→ **SEL6**/**SEL7**). The last one corrected a call this plan got wrong.

## Context

① CAMERA closed 2026-08-26, user-verified (`cda4559`, tree clean, 8 ahead unpushed).
`.claude/plans/engine-sequence.md` §② is next and its content is not to be re-derived: **screen→world
mouse · viewport click-select · focus-selected · create entity · multiselect.**

Today the viewport is display-only — the world renders into an FBO shown as an ImGui image
([ViewportPanel.cpp](Editor/Source/Editor/Panels/ViewportPanel.cpp)) — and the only way to make an entity
is `SandboxPanel`'s **Spawn Quad**, which calls `World::CreateEntity(name)` with no `OwnerMap`, so its
entities land in `(runtime - not saved)` and **no Save can ever write them** (**WM2**).

① left the starting half: `ScreenToWorld` ([CameraView.h:45](Engine/Source/Renderer/CameraView.h)) is
exported and tested. ① paid **none** of §②'s standing objection — cutting camera-follow removed its claim
on an entity-position helper, so nothing answers "where is this entity" yet.

### What planning turned up, and what it changed

Planning a bare `Create Entity` exposed a hole the user caught: **an entity with no sprite has nothing to
click on.** Checking the reference engines showed why, and the answer is structural rather than cosmetic:

| | universal position | what you click on an "empty" object |
|---|---|---|
| **Unreal** | every `AActor` has a root `USceneComponent` | editor-only `UBillboardComponent` (`S_Actor`, camera glyphs), constant screen size, picked by hit proxies |
| **Unity** | every GameObject **must** carry a `Transform` | a gizmo icon at the transform; scene picking includes icons |
| **Godot** | `Node2D` carries position | the 2D editor asks each node `_edit_get_rect()`; no rect → a small fixed grab area at the origin |

All three hang the icon off a transform **every** object is guaranteed to have — Unity cannot even express
a GameObject without one. This engine has no such guarantee: `Position` lives separately on
`SpriteComponent`, `DummyComponent` and `CameraComponent`, so an entity carrying none of them has no
position at all. Godot's `_edit_get_rect()` is exactly the one-helper this block was told to build; the
tier below it needs an anchor that does not exist.

**Decisions taken with the user this session:**
1. **Pull `TransformComponent` forward into ②** and fold `Position` out of the three components in the
   same change. ③ becomes gizmo-only. This is consistent with their own sequencing rule — ② is what makes
   the transform visible (memory `sequence-by-what-becomes-visible`).
2. **The icon is a wireframe box via `DebugDraw`** at constant screen size — no new render surface.
3. **Multiselect is Ctrl-toggle plus a drag marquee** (no shift-range). The box is *built*, not merely
   shaped for: an untested `QueryOverlapping` with no caller is precisely the shape [[L23]] forbids.
4. **Delete Entity ships with Create.**
5. **`SandboxPanel` is deleted**, not repaired.
6. **Map migration: split the camera, snap the sprites** (below).

---

## Step 1 — the transform fold

**`Engine/Source/World/Components/TransformComponent.h`** — new. `Position` (Vector2F) + `Rotation`
(float, degrees — what an author types; converted at the draw call, which already takes radians).
`Scale` is a **named growth point, not built**: `Size` on Sprite/Dummy is the extent and nothing needs a
multiplier yet (**X5** — a field nothing reads is a spec). Carries
`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` and `OPAAX_PROPERTIES` like every other component (**I8**,
**I15**), so it costs the Inspector nothing.

**`World::CreateEntity` / `CreateEntityWithGuid`** emplace it beside `EntityMeta` — the Unity rule. That is
what makes `Each<TransformComponent>` complete and *every* entity anchorable, which is the whole point of
the icon. **Verified safe on load:** `IComponentEntry::Load` uses `get_or_emplace`
([ComponentRegistry.h:89](Engine/Source/World/Components/ComponentRegistry.h)), so a map's payload fills
the auto-added component instead of asserting. Registered in
[Engine.cpp:72](Engine/Source/Engine/Engine.cpp) as `Register<TransformComponent>("Transform")`, beside
the other three — the short key the file format already uses.

**`Position` is DELETED** from `SpriteComponent`, `DummyComponent` and `CameraComponent`. The consumers are
bounded — this is the whole sweep, taken from the repo root with `Legacy/` excluded, tests included
([[L10]], three strikes on scoping that grep to source dirs):

- `RendererManager.cpp:163` and `:200` — the quad and sprite passes become `Each<TransformComponent, X>`
  joins (`World::Each<A,B>` already exists) and gain rotation, which `DrawQuad`/`DrawSprite` already take.
- `CameraManager::Resolve` — the `Each<CameraComponent>` walk becomes the same join.
- `ViewportPanel::EnqueueSelectionOutline` — rewritten onto `EntityQuery` in step 3 anyway.
- `Sandbox` `QuadBoundsSubsystem.cpp:39` (Edit overlay) and `QuadOscillatorSubsystem.cpp:41/82/104`
  (Play, writes `Position.y`) → read and write the Transform.
- Four test files: `CameraResolveTests`, `MapFileTests`, `MapSnapshotTests`, `WorldCloneTests`.
- `SandboxEditorModule.cpp` registers a drawer for it (one line — every field type is already covered).

### The map migration

Three of the seven authored entities carry two positions; four fold trivially.

| Map · Entity | Dummy | Sprite | Camera | becomes |
|---|---|---|---|---|
| Main · QuadWhite | (0,0) | — | — | Transform (0,0) |
| Main · **QuadBlue** | (175,0) | (179,−64) | — | Transform (175,0) — sprite snaps |
| Main · **QuadRed** | (−200,0) | — | (0,0) | Transform (−200,0); **camera splits onto a new `MainCamera` entity at (0,0)** |
| Arena · ArenaFloor | (0,−160) | — | — | Transform (0,−160) |
| Arena · **ArenaPillar** | (303,40) | (0,0) | — | Transform (303,40) — sprite snaps off the world origin |
| Decor · DecorTopGreen | (0,73) | — | — | Transform (0,73) |
| Decor · DecorBottomYellow | (241,−6) | — | — | Transform (241,−6) |

Splitting the camera preserves the framing **exactly** and is the model all three reference engines have
(a camera somewhere else is a different object). The two sprite snaps are the only visible change, and both
look like a component added in the Inspector and never moved. A per-sprite local `Offset`
(Godot's `Sprite2D.offset`) is named here as the growth point for genuine art offsets — **not built**.

`_WITH_DEFAULT` (**I8**) means a stale key is *ignored*, not rejected, so an unmigrated map would load
silently at the origin — the migration is mandatory, not tolerated. The three files must match the writer
**byte-for-byte** (**MP6**: nlohmann `dump(4)`, keys **sorted**, **no trailing newline**); verify with
`od -c` plus `json.dumps(sort_keys=True, indent=4)`. Python 3.14 is on PATH as `python3` — **write the
script to a file**, a heredoc mangles escapes, then delete it.

**Gate for this step: the two hosts render exactly as they do now**, bar the two snapped sprites. Full
tests green.

## Step 2 — the one bounds rule (engine, unit-tested)

Nothing here needs a GL context, which is the point: it is the only part of ② a test can reach
(`OpaaxTests` cannot see editor code — a standing M2a gap).

**`Engine/Source/Core/Maths/Bounds2D.h`** — new, header-only, **no `OPAAX_API`** (**I6**: a stateless
value type is DLL-safe by construction). `Center` + `HalfExtent` in world units, with `Contains` (point
pick), `Intersects` (marquee), `Encapsulate` (focus on a multi-selection), `FromCenterSize` and
`FromMinMax` — the last because a drag runs in any of four directions, so the marquee's two corners are
normalised in one place rather than at the call site. In `Core/Maths/` because it is geometry owned by no
contract (**I9**).

**`Engine/Source/World/Entity/EntityQuery.{h,cpp}`** — new, **`OPAAX_API`**, out-of-line. Exported the day
it is written because `SandboxEditor.exe` calls it: **I6**'s three-strike checklist paid ahead, exactly as
① paid it for `ScreenToWorld`.

```cpp
namespace Opaax::EntityQuery
{
    OPAAX_API bool   TryGetBounds(Entity& InEntity, Bounds2D& OutBounds, float InAnchorHalfExtent = 0.f);
    OPAAX_API bool   TryGetBounds(World&, const TDynArray<EntityID>&, Bounds2D&, float InAnchorHalfExtent = 0.f);
    OPAAX_API Entity PickAt(World& InWorld, const Vector2F& InWorldPoint, float InAnchorHalfExtent = 0.f);
    OPAAX_API void   QueryOverlapping(World&, const Bounds2D& InRegion, TDynArray<EntityID>& OutIds,
                                      float InAnchorHalfExtent = 0.f);
}
```

`PickAt` and `QueryOverlapping` are the same walk over the same tiers, differing only in
`Contains` versus `Intersects` and in taking one answer versus all of them — which is why they belong in
one file rather than growing apart. Two tiers, and this `.cpp` is **the only place** an entity's extent is
read for a hit test, so the gizmo and anything after it change one body:
1. extent-bearing components (`Sprite`, `Dummy`) → the union of their boxes, centred on `Transform.Position`.
2. none → a box of `InAnchorHalfExtent` at `Transform.Position`. **`0.f` means "no anchor fallback"**,
   which is what a game or a test wants; the editor passes a real number.

`PickAt` is topmost by `(ERenderLayer, OrderInLayer)` — the renderer's own sort order
([RenderLayer.h](Engine/Source/Renderer/RenderLayer.h)); a `Dummy` reads as `Default`/0 and an anchor-only
entity sorts last. Ties go to iteration order: arbitrary but stable, and said out loud.

**Tests:** `Engine/Tests/Core/Maths/Bounds2DTests.cpp` and `Engine/Tests/Core/World/EntityQueryTests.cpp`
against a bare `World` — covering both tiers, the topmost rule, and a marquee that catches an anchor-only
entity. Both must be added to `Engine/Tests/CMakeLists.txt`: test sources are listed **explicitly**, not
globbed.

## Step 3 — click-select, box-select, icons

**`Editor/Source/Editor/Operation/EditorViewport.hpp`** — new, the `EditorSelection` shape: the viewport's
last measured content size in pixels, written by `ViewportPanel::DrawContents` (which already measures
`GetContentRegionAvail`), read by the focus verb in step 4. Joins `EditorContext` for the reason **I16**
gives for `ResourcePreview` — the writer (a panel) and the reader (a menu command) are different objects,
and a command has no other route to a panel's private size.

**`EditorSelection`** becomes set-valued with the existing surface kept meaning what it means, so no
current caller changes: `Select(Entity)` replaces the set with one · **new** `Toggle(Entity)` (Ctrl+click),
`Add(Entity)` and `Replace(const TDynArray<EntityID>&, World&)` (the marquee's two outcomes) ·
`Get()` still answers the **primary** (last touched), which is what leaves `InspectorPanel` untouched and
keeps multi-**edit** out of this block · `Contains`, `Ids()`, `Count()`.
`EditorService::HandleActiveWorldChanged` already retargets by Guid — it retargets **every** entry now.

**`ViewportPanel`** — the pick reuses the measure-then-apply handshake the panel already runs twice:

- `DrawContents` banks a left click over the image as `{viewport-local px, bCtrl}`, beside
  `MeasureCameraGesture`, gated on this window's own hover — **never `io.WantCaptureMouse`**, which is
  permanently true over the viewport ([[L29]]).
- `OnPreRender` spends it **FIRST — before `ApplyPendingResize` and `ApplyCameraGesture`** — so the pick
  reads the exact viewport size and `CameraView` the clicked frame was rendered with. That ordering is the
  correctness argument and gets the comment.
- The view comes from `World::GetCameraView()`, not from `EditorCamera`, so **picking needs no Edit/Play
  fork**: it asks the world how it was framed, and works inside a PIE session for free.
- Plain click replaces, Ctrl+click toggles, plain click on empty space clears.

**The drag marquee** is the same gesture with one more state, and it shares the banked-click machinery
rather than adding a second one:

- A left press banks the start pixel as *pending*. It becomes a **box** only once the pointer passes
  ImGui's own `io.MouseDragThreshold` (`IsMouseDragging`), so a click stays a click — one threshold, and it
  is the one the rest of the UI already uses.
- While it is a box, `DrawContents` paints it with `GetForegroundDrawList()->AddRectFilled` + `AddRect`
  in **screen** pixels. Deliberately not a world-space `DebugDraw`: a marquee is UI, and drawing it in the
  same pass that measures it is what keeps it free of the one-frame lag everything else here lives with.
- On release `OnPreRender` converts both corners through `ScreenToWorld` into one `Bounds2D::FromMinMax`
  and calls `EntityQuery::QueryOverlapping`. Ctrl held → `Add` each; otherwise `Replace`.
- The middle-button pan is untouched — different button, and `m_bPanning` already owns its own state.

**The anchor size is computed once and used twice**, which is what makes what-you-see-what-you-click true
rather than approximately true: `anchorHalfExtent = (OrthoSize / viewportHeightPx) * ICON_HALF_PIXELS`,
handed to both `EntityQuery::PickAt` and the icon draw.

- **`EnqueueEntityIcons()`** — new, beside `EnqueueSelectionOutline` in `OnPreRender`, **Edit worlds only**
  (an editor overlay must not decorate a running game). One `DebugDraw::DrawBox` per entity with no
  extent, at its `Transform.Position`, at the anchor size.
- **`EnqueueSelectionOutline`** loops the whole selection through `EntityQuery::TryGetBounds`. That also
  closes a live bug: it reads `DummyComponent` only today, so a **sprite-only entity gets no outline**.

## Step 4 — `EntityOps`, three commands, and the panel deletion

**`Editor/Source/Editor/Operation/EntityOps.{h,cpp}`** — new, mirroring
[MapOperations.h](Editor/Source/Editor/Operation/MapOperations.h) deliberately: same shape, same "every
verb takes its target" rule, same stated reason (*"a verb duplicated per call site is a verb that
drifts"*). **This is ⑤'s one named mutation choke point**, and naming it now is what makes undo a wrapper
instead of a twenty-call-site hunt.

```cpp
namespace Opaax::Editor::EntityOps
{
    Entity Create(EditorContext&, MapId InOwnerMap, const OpaaxString& InName);
    void   DestroySelected(EditorContext&);
    void   FocusSelected(EditorContext&);
}
```

`Create` takes `MapId` **as a required argument** — that is how **WM2** is closed by construction rather
than by care. It selects what it made and calls `World::MarkChanged()`. Both mutators gate on the existing
`MapOps::CanEdit`; `FocusSelected` refuses outside Edit with the same Warn, because a Play world is framed
by its `CameraComponent` and moving the editor camera there would silently do nothing.

**`EditorCamera::FocusOn(const Bounds2D&, const Vector2F& InViewportPx)`** — position = centre,
`OrthoSize = max(half.y, half.x * height/width) * margin`, floored at a minimum. It joins `Pan` and
`ZoomAtCursor` in the type's existing vocabulary (memory `extend-the-existing-idiom`), speaking the same
world units.

**Three commands**, each following `QuitCommand` exactly — struct in
[EditorNativeCommands.h](Editor/Source/Editor/Commands/EditorNativeCommands.h), tag in
`EditorNativeCommandsTags.hpp`, `Register<T>` in `RegisterNativeEditorCommand`, entry in
`RegisterNativeMenus`: `CreateEntityCommand` (into `MapDocument.GetMapId()`), `DeleteSelectedCommand`,
`FocusSelectedCommand`, under a new **Edit** category with `SetEnabled(IsEditing)`.

**Hierarchy** — a map header's context menu gains **Create Entity**, where *the map you clicked IS the
`OwnerMap` argument*, which is `MapOperations.h`'s own stated rationale for existing. An entity row gains
**Delete**. Both extend the existing `PendingMapAction` deferral: calling straight through destroys the
entities the loop is mid-way through drawing.

**Keys** are measured in `ViewportPanel::DrawContents` while its window is hovered/focused and dispatched
**by tag**, so a key and a menu entry reach one verb: `F` → focus, `Delete` → delete. Deliberately **not**
in `HandleAuthoringShortcuts` beside Ctrl+S: that uses `ImGuiInputFlags_RouteGlobal`, and an unmodified
`F` on a global route would fire while a text field elsewhere owns the keyboard. The viewport gate is what
makes a bare key safe — the same reason the camera gesture is measured there (**IN8**/[[L29]]).

**Delete `SandboxPanel`** in the same commit, since Create Entity is what replaces it: remove
`Sandbox/Editor/Source/SandboxEditor/Panels/SandboxPanel.{h,cpp}`, its `#include`, and the
`Panels().Register<SandboxPanel>` block in `SandboxEditorModule.cpp`.

> **Blast radius, stated: it is the ONLY game-module panel.** The `PanelRegistry::Register<T>` route stays
> exercised by the editor's eight native panels, but M2's dogfood gate — *"a Sandbox-module custom panel
> appears with zero changes to `OpaaxEditorLib`"* — stops being live. The module's other four dogfoods (a
> world subsystem, a command, a hand-written drawer, a resource type) are untouched.

---

## Verification

Commit at **each step boundary** ([[L17]]), and read `git status --short` *after* staging — the slice adds
new files, so a bare `git commit` after `git add` silently drops every modified one ([[L9]] corollary).
Sign "Claude"; never sign a commit that is the user's own content.

**Machine gates, every step:**
- `./build.bat` on all three presets; grep `OPAAX_BUILD_OK` / `OPAAX_BUILD_FAIL` — **the exit code lies**
  ([[L8]]). `OPAAX_NO_PAUSE=1`; output is buffered, so wait rather than poll.
- `OpaaxTests.exe` directly (CTest reports "1 test"). Baseline **401 / 6807 / 7**; step 2 must raise the
  first number.
- `ls -la` the hosts against the DLL before any smoke run ([[L24]]), and **back up
  `Sandbox/Editor/Save/imgui.ini` before every editor launch and restore after** — a smoke run flushes my
  window geometry into the user's dock layout.
- Both hosts smoke clean, reading the whole log. `Main.opaaxlevel` names a `Maps/Sprites.opaaxmap` that
  does not exist: **4 errors on every boot, pre-existing, not a regression.**

**Eye gates — the user's; the block does not close without them.** Each has something to look at and a log
line behind it ([[L12]]):
1. After step 1: both hosts render as before, except QuadBlue's and ArenaPillar's checkers now sit on their
   quads. QuadRed still framed by the camera, now a separate `MainCamera` entity in the Hierarchy.
2. Click a quad in the viewport → its Hierarchy row highlights, an orange outline appears, the Inspector
   fills. Click empty space → cleared.
3. Click a **sprite** (not a quad) → it outlines too. It does not today.
4. An entity with no sprite or quad draws a small wireframe box that stays the same size on screen as you
   zoom — and clicking that box selects it.
5. Ctrl+click a second entity → **two** outlines; the Inspector still shows one.
6. Left-drag across empty space → a marquee draws under the cursor; release selects everything it touched,
   icon-only entities included. Ctrl+drag adds to what was already selected. A quick click inside the drag
   area is still a plain click, not an empty box.
7. `F` over the viewport → the camera frames the selection; with several selected it frames all of them.
8. Right-click a map header → **Create Entity** → the row appears **under that map**, not under
   `(runtime - not saved)`. It draws as an icon box immediately. Add a `SpriteComponent`, Ctrl+S, reopen —
   it is in the `.opaaxmap`.
9. Select it, `Delete` → gone; the map goes `*` dirty.
10. Play (F5) → picking still works in the running world; Create/Delete/Focus refuse with a Warn naming
    the verb.

**Records updated in the same change** (CLAUDE.md §0 — the contract is fixed *with* the code): a plan at
`.claude/plans/editor-enhancements.md`, new ARCHITECTURE.md entries for the transform rule, the one-bounds
rule and the choke point, `engine-sequence.md` §③ corrected to gizmo-only, `todo.md` rewritten to owe only
what is left, and lessons promoted from `.claude/task/lessons.md` when the block closes.
