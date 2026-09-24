# Plan — Editor M2 "Panels & extensions" (program overview)

> **Provenance:** designed on the **Fable** model (Plan agent) 2026-07-26; **restructured after review**
> (Opus, same day) into four independently-shippable slices. Base: `51b2b14` + the `SandboxApp.cpp`
> unblock (dead `ConfigTest` reference left by that commit; fixed + verified green, 85/354/2).
> Conforms to `Docs/Architectures/Editor.md` (D1–D10, §5 M2 row) + `.claude/ARCHITECTURE.md` (I1–I6, LC,
> F, SE, MR, X). If any step disagrees with those, they win.
>
> **This file is the program overview** — the decomposition, the dependency graph, and the decisions
> shared by every slice. Each slice gets its own detailed plan when it starts (M1 precedent:
> `.claude/plans/m1-viewport.md`). Live tracking → `.claude/task/todo.md`.
>
> | Slice | Detailed plan | Status |
> |---|---|---|
> | M2a — panel infra + Hierarchy | `.claude/plans/m2a-hierarchy.md` | ✅ `e76a60e`, `875bc85` |
> | M2b — Drawers + Inspector | `.claude/plans/m2b-inspector.md` | ✅ `add4959`, `3b03d08` |
> | M2c — DebugDraw + selection outline | `.claude/plans/m2c-debugdraw.md` | ✅ `c613bdf`, `a473c12` |
> | M2d — **ResourceTypes + ResourceBrowser** | `.claude/plans/m2d-resourcebrowser.md` | ✅ `fcc7c1a`, `0593fa0`, `083504b` |
>
> **M2d renamed the route Asset→Resource** (user, 2026-07-27): `Asset` is the retired `Legacy/Assets`
> vocabulary, the live engine says `CResource`/`ResourceManager`. `Docs/Architectures/Editor.md` D10 was
> amended in the same change (route name, descriptor call site, M2 milestone row). Wherever this file
> still says `AssetTypes()` / AssetBrowser below, read `ResourceTypes()` / ResourceBrowser — the
> reasoning (F1 atomicity, F3 content-row, §3.5 file-browser-not-catalog) is unaffected.

---

## 1. Why M2 is split (review findings)

Editor.md §5 lists M2 as one milestone: "Hierarchy, Inspector, AssetBrowser; `Drawers()`/`Panels()`/
`AssetTypes()` consumed — native panels register through the same path as game panels; engine
`DebugDraw` API." Editor.md is the design authority on **what** M2 contains; how it sequences into
buildable steps is this plan's job (same as M1's plan sequencing S0–S4). Three findings forced a split:

**F1 — Registry, panel, and dogfood are atomic; they cannot be separate steps.** The original plan had a
step landing the real `DrawerRegistry`/`PanelRegistry` while leaving `SandboxEditorModule`'s M0
placeholders in place, gated on "the placeholders still compile." They do not, by inspection:
- `Drawers().Register<int,int>()` instantiates the stored lambda with `TDrawer = int` →
  `int lDrawer; lDrawer.Draw(*lComp);` → ill-formed.
- `Panels().Register("Sandbox Panel", [](EditorContext&){ return 0; })` → `int` is not convertible to
  `UniquePtr<IEditorPanel>` → ill-formed.

So the moment a route graduates from counts-only to real storage, its consumer **and** its Sandbox
placeholder must move in the same step. Each registry therefore anchors its own slice.

**F2 — Hierarchy needs no new engine API; AssetBrowser needs the most.** Verified: `World::CreateEntity`
unconditionally emplaces `EntityMeta{Guid, Name}` (`World.cpp:51`), so `World::Each<EntityMeta>()` **is**
the all-entities view by construction — Hierarchy enumerates with zero World/entt additions. That makes
it the cheapest slice and the natural carrier for the shared panel infrastructure (which cannot ship
alone: pure infra has no observable gate — L12).

**F3 — DebugDraw and AssetBrowser are both content-row, not gate-row.** Editor.md's M2 gate is only
"click-select + live transform edit; a Sandbox-module custom drawer AND custom panel appear with zero
changes to `OpaaxEditorLib`" — it never exercises AssetBrowser, AssetTypes, or DebugDraw. The original
plan deferred AssetBrowser on exactly this reasoning but kept DebugDraw, which is inconsistent. Both get
their own slice, each built **with** its first real consumer (never an API with no caller).

---

## 2. Dependency graph

```
        ┌─────────────────────────────────────────┐
        │ M2a  panel infra + Hierarchy            │  needs: nothing new engine-side
        │      PanelRegistry · m_Panels           │  (Each<EntityMeta> already exists)
        │      EditorSelection · EditorContext++  │
        └──────────────┬──────────────────────────┘
                       │ selection + panel collection
          ┌────────────┴───────────┬──────────────────────┐
          ▼                        ▼                      ▼
   ┌─────────────┐        ┌────────────────┐      ┌────────────────┐
   │ M2b         │        │ M2c            │      │ M2d            │
   │ DrawerReg   │        │ DebugDraw      │      │ AssetTypes     │
   │ + Inspector │        │ + sel. outline │      │ + AssetBrowser │
   └─────────────┘        └────────────────┘      └────────────────┘
   needs a component      needs selection          needs a file listing
   (DummyComponent ✓)     (from M2a)               (editor-side scan ✓)
```

- **M2a is the only hard prerequisite.** M2b/M2c/M2d each depend on it and on nothing else; after M2a
  they can be built in any order (the table order is the recommendation, not a constraint).
- **M2c's engine half (`DebugDraw`) has no dependency at all** — it could land any time. It is sequenced
  after M2a only so it ships with a real consumer (the viewport selection outline) rather than as a
  dead API.

---

## 3. Decisions shared by every slice (CH — decided up front)

1. **Inspector edits `DummyComponent`, not a new `TransformComponent`.** It is the only component
   `RendererManager::Render()` actually reads (`Each<DummyComponent>` → `Renderer2D::DrawQuad`,
   `RendererManager.cpp:129`). A new transform type would either duplicate data nothing draws or force a
   render-path change inside a panels milestone (L3). `ComponentRegistry` v2 with D7's authoring/runtime
   classification is confirmed **M3** scope; introducing a "real" component ahead of the system meant to
   organize it is premature. `Register<TComponent, TDrawer>()` is a template — swapping the component
   later is a one-line call-site change, not a rewrite.
2. **Selection lives in a new `EditorSelection` type**, owned by `EditorService`, referenced from
   `EditorContext`. **Not** named `EditorState` — Legacy has an `EEditorState{Editing,Playing,Paused}`
   PIE state machine, a different concept that M4 will want the name for; X2/X3 says give the new type a
   collision-proof identity up front rather than discover the clash later. Not stored in
   `World`/`WorldManager` either — selection has zero runtime meaning (`Sandbox.exe` never needs it).
3. **`ViewportPanel` stays a specially-named member**, not folded into the generic `m_Panels` collection.
   It is the one panel whose lifecycle is load-bearing beyond "a dockable window": it drives
   `IEngine::SetPrimaryRenderTarget`, and its teardown must precede `m_Context.reset()` (M1). Once the
   collection grows a close/erase affordance (M5 layout persistence is the natural trigger), a stray
   close would otherwise tear down the render target — a bug class a named member is structurally immune
   to. Reversible later; nothing here blocks folding it in.
4. **`DebugDraw` is engine-owned, not editor-owned.** Editor.md D10's own wording — "serves editor
   overlays *and* dev builds of `Game.exe`" — rules out `OpaaxEditorLib` ownership outright, since
   `Sandbox.exe` never links the editor lib. Details in M2c's plan.
5. **AssetBrowser is a file browser, not an asset catalog** (user decision, 2026-07-26). An editor-side
   directory scan of the project's Assets dir; `AssetTypes()` maps file extension → icon / label /
   double-click action. Zero engine change, so it needs no M3 prerequisite. The GUID-backed catalog
   (`.meta` sidecars, rename-safe references, thumbnails) is the later upgrade, naturally driven by M3
   when scene files need stable asset IDs — not built now.

---

## 4. Verified starting state (shared; read, not assumed — L13/L14)

- **Baseline re-established this session:** `debug-editor` `OPAAX_BUILD_OK`, **85 cases / 354 assertions
  / 2 skipped**, after unblocking `SandboxApp.cpp` (commit `51b2b14` deleted `Config/ConfigTest.h` but
  left its `#include` + `Register<Config_MyConfig>()` call). The empty `OnInitializeApplication()`
  override was deliberately **kept** — deleting it lets the base's "not overridden" trace fire, which
  would change `Sandbox.exe`'s log and break the D4 byte-identical gate every slice relies on.
- **`IEditorPanel`** (`Editor/Source/Editor/Panels/IEditorPanel.h`): `Startup/OnPreRender/Draw/Shutdown`
  + `GetPanelID() const -> OpaaxStringID`. `ViewportPanel` is the only implementation; `EditorService`
  owns it by name and calls its hooks directly. **No generic panel collection exists.**
- **`EditorExtensionRegistrar`**: all five routes (`Drawers/Panels/AssetTypes/Menus/EditWorldSystems`)
  share one counts-only `EditorRoute`. `Seal()`/`IsSealed()` exist (no late-registration assert).
  `EditorService::RegisterExtensions()` — fired from the existing `OnModulesRegistered` seam, before the
  first world — invokes the game's `IEditorModule::OnRegister`, seals, and logs the five counts.
- **`SandboxEditorModule.cpp`**: all five registrations are `int`/`return 0` placeholders (see F1 —
  each must be replaced in the slice that makes its route real).
- **`EditorContext`**: exactly `IEngine&`, `WorldManager&`, `ResourceManager&`, `IEditorUIBackend&`.
  No selection/state/eventbus member yet. Built once in `EditorService::Initialize()`
  (`PostEngineStartup`), injected by ctor into every panel (D3 — panels never see the locator).
- **World/ECS**: entt-backed. `Each<T>` / `Each<A,B>` typed views; **no all-entities API needed** (F2 —
  `Each<EntityMeta>` covers it). `Entity` is a `(EntityID, World*)` handle with
  `Add/Get/TryGet/Has/Remove<T>()`, `GetGuid()`, `IsValid()`. `ComponentBase`/`IComponent` are empty
  non-virtual markers (value types, no vtable — matches D7 "no OOP components"). The only live component
  is `DummyComponent{Position, Size, Color}`.
- **Rendering**: `Renderer2D` exposes exactly one primitive, `DrawQuad(pos,size,color,rot,layer,order)`.
  `ICommandBuffer` has no line primitive. **No `DebugDraw` exists** (repo-wide search empty).
  `RendererManager` owns `RenderSystem` + `m_PrimaryTarget` (M1) and drives `Render(double)`.
- **No live asset catalog**: `ResourceManager` is a load-by-path pool with no enumerate capability; all
  asset-listing code (`AssetRegistry`, `AssetScanner`) is unlinked Legacy (X1). Hence decision §3.5.
- **Legacy reference** (X1, reference-only, do not migrate or depend on): old
  `Hierarchy`/`Inspector`/`AssetBrowser` panels (event-bus + `WorldOld` coupled — layout ideas only),
  `IComponentDrawer` (OOP virtual dispatch — the shape D7 declines), `IAssetTypeActions` (concept
  salvageable, rewritten injected), `EditorState.h` (see decision §3.2).
- **CMake**: `Editor/CMakeLists.txt` and `Sandbox/CMakeLists.txt` both `GLOB_RECURSE ... CONFIGURE_DEPENDS`,
  so new files under `Editor/Source/Editor/**` and `Sandbox/Editor/Source/SandboxEditor/**` are picked up
  with **zero CMakeLists edits** (as M1 found for `ViewportPanel`).

---

## 5. Slice summaries

Each slice: builds green at every step, ends in an observable demo gate (L12), and keeps `Sandbox.exe`
**byte-identical** (D4) — none of M2 touches the runtime path. Every slice's dogfood diff must touch only
`Sandbox/Editor/**`, which is the checkable form of Editor.md's "zero changes to `OpaaxEditorLib`."

### M2a — panel infrastructure + Hierarchy → `.claude/plans/m2a-hierarchy.md`
`PanelRegistry` (real storage: name + factory), `EditorService` grows a generic
`TDynArray<UniquePtr<IEditorPanel>> m_Panels` built from it, `EditorSelection`, `EditorContext` growth,
`HierarchyPanel` (native, registered through the same route as game panels), and the Sandbox custom panel
(F1: same step). **Gate:** Hierarchy lists `QuadRed`/`QuadGreen`/`QuadBlue`; clicking highlights the row;
"Sandbox Panel" appears docked and its button logs.

### M2b — Drawers + Inspector
`DrawerRegistry` (type-erased `Register<TComponent, TDrawer>()` → closure; duck-typed drawer contract,
one method `void Draw(TComponent&)`, **no** `IComponentDrawer` base — D7), `InspectorPanel`, and the
Sandbox `DummyComponentDrawer` (F1: same step). **Gate:** select in Hierarchy → Inspector shows
Position/Size/Color → drag → the quad moves in the viewport (one frame later; documented lag, same class
as M1's viewport-resize lag — flag it in the demo so it does not read as a bug).

### M2c — DebugDraw + selection outline
Engine `Renderer/DebugDraw.h` (`DrawLine`/`DrawBox` only — no circle until something needs one), owned by
`RendererManager`, reached via `IEngine::GetDebugDraw()`; lines render as thin rotated quads through the
**existing** `Renderer2D::DrawQuad` (zero new RHI/shader/vertex-layout surface). First consumer:
`ViewportPanel` outlines the selected entity. **Gate:** selected quad visibly outlined, zero frame lag
(enqueued in `BeginFrame`, before `Engine().Loop()`); `Sandbox.exe` byte-identical.

### M2d — AssetTypes + AssetBrowser (file browser — §3.5)
`AssetTypeRegistry` (mirrors `DrawerRegistry`'s shape, keyed by file extension → icon / label /
double-click), `AssetBrowserPanel` listing the project's Assets dir, and a Sandbox asset-type
registration (F1: same step). **Gate:** the browser lists real files from disk; a registered extension
shows its label/icon and responds to double-click.

---

## 6. Verification (applies to every slice)

1. **3 presets** grep `OPAAX_BUILD_OK` (`debug-editor` / `release` / `release-editor`).
2. **Tests:** record the exact count per slice; baseline is 85/354/2. Only M2c is expected to add engine
   tests (`DebugDrawTests.cpp`, pure data, no GL — mirrors M1's `StubFramebuffer` pattern); the editor lib
   is not linked into `Engine/Tests`, so M2a/M2b/M2d leave the suite unchanged.
3. **`Sandbox.exe` byte-identical (D4)** every slice: 3 quads, 1280x720, 0 err/warn, no new log lines.
4. **`SandboxEditor.exe`** observable gate per slice (§5), each with a log line or a specific UI
   interaction to point at — never "no errors" alone (L12).
5. **Dogfood diffs touch only `Sandbox/Editor/**`** — `git diff --stat` is the check.
