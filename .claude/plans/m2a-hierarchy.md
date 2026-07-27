# Plan — M2a "Panel infrastructure + Hierarchy"

> **Provenance:** first slice of the M2 program — see `.claude/plans/m2-panels.md` for the decomposition,
> the dependency graph, the decided cross-slice forks, and the shared verified starting state (not
> repeated here). Base: `51b2b14` + the `SandboxApp.cpp` unblock. Structure follows the M1 precedent
> (`.claude/plans/m1-viewport.md`). Live tracking → `.claude/task/todo.md`.

---

## 1. Scope

The shared panel infrastructure every later slice depends on, delivered attached to the first visible
panel (infra alone has no observable gate — L12). After M2a, `EditorService` no longer knows any panel by
name except `ViewportPanel`: panels are **registered** (name + factory) and constructed generically, and
native editor panels travel the exact same route as game panels — the mechanism Editor.md D10 specifies
and M2b/M2c/M2d all build on.

**Out of scope, by slice:** drawers/Inspector (M2b), `DebugDraw` + selection outline (M2c), asset
types/browser (M2d). Selection is *stored and set* here; nothing renders a highlight in the viewport
until M2c.

---

## 2. New types + changes (each checked vs invariants)

### 2.1 `EditorSelection` (NEW — `Editor/Source/Editor/EditorSelection.h`)

Header-only, four methods: `Select(Entity)`, `Clear()`, `Get() const -> Entity`,
`HasSelection() const -> bool`. One member, `Entity m_Selected` — default-constructed is already the
invalid state (`Entity::IsValid()` is `m_World != nullptr && m_World->IsValid(m_Handle)`), so no sentinel.

- **I1** no static · **I5** owned by `EditorService` (`UniquePtr<EditorSelection>`), referenced from
  `EditorContext` · **I2** n/a — `OpaaxEditorLib` is a **static** lib linked wholly into
  `SandboxEditor.exe`; no editor-only type in this slice crosses the engine DLL boundary · **I6** not a
  template.
- **Known limitation, deliberate:** `Entity` holds a raw `World*`, so a selection outliving its world
  dangles. M2a has exactly one world, created at startup and alive for the engine's lifetime, so this is
  unreachable today. `WorldManager` already broadcasts world-destroyed events (`Engine::HandleWorldDestroyed`
  is wired), so the fix is a subscription — but it belongs to **M4**, where PIE actually creates and
  destroys worlds. Record as `// FIXME (M4): clear selection on WorldDestroying` on the member, not built now.

### 2.2 `PanelRegistry` (NEW — `Editor/Source/Editor/Extensions/PanelRegistry.h`)

Replaces `EditorRoute` behind `EditorExtensionRegistrar::Panels()`:

```cpp
using FPanelFactory = TFunction<UniquePtr<IEditorPanel>(EditorContext&)>;

struct PanelEntry { OpaaxStringID Id; FPanelFactory Factory; };

void                         Register(const char* InName, FPanelFactory InFactory);
const TDynArray<PanelEntry>& Entries() const noexcept;
Uint64                       Count()   const noexcept;   // keeps the existing seal log unchanged
```

Registration **stores only** — nothing is constructed. It must: the `OnModulesRegistered` seam that fires
`RegisterExtensions` runs *before* `Engine().Startup()`, so no `EditorContext` exists yet. Factories are
invoked once later, in `Initialize()` (§3). Interning the name to `OpaaxStringID` at registration matches
`IEditorPanel::GetPanelID()`, so a registered panel and its instance share one identity.

Note this type is **not** itself a template (unlike M2b's `DrawerRegistry`) — `Register` takes a runtime
name + type-erased factory. The call-site shape is unchanged from the M0 skeleton (MR1/D10); only the
factory's return type stops being the placeholder `int`.

- **I1/I2/I5/I6** as §2.1 (owned by `EditorExtensionRegistrar`, itself a by-value `EditorService` member).

### 2.3 `EditorExtensionRegistrar` (MODIFIED)

`Panels()` returns `PanelRegistry&` instead of `EditorRoute&`. **`Drawers()`, `AssetTypes()`, `Menus()`,
`EditWorldSystems()` stay exactly `EditorRoute&`** — M2b/M2d/M5/M4 respectively; untouched here.
`Seal()`/`IsSealed()` unchanged, and the existing five-count seal log needs no edit (`Count()` has the
same signature on both types).

### 2.4 `EditorContext` (MODIFIED)

Grows one member, built before `m_Context` in `Initialize()` — the same reorder-and-grow pattern M1 used
for `UIBackend`:

```cpp
EditorSelection& Selection;   // NEW — Hierarchy writes, Inspector (M2b) reads
```

M2b adds the `Extensions` reference when `DrawerRegistry` gives panels a reason to read a route at
runtime; nothing in M2a needs it, so it is not added speculatively.

### 2.5 `HierarchyPanel` (NEW — `Editor/Source/Editor/Panels/HierarchyPanel.{h,cpp}`)

An `IEditorPanel`. `Draw()` enumerates the active world via `Each<EntityMeta>` (the all-entities view by
construction — see the overview's F2), emits one `ImGui::Selectable` per entity showing `Meta.Name`,
marks it selected when its handle matches `Context.Selection.Get().GetHandle()`, and on click calls
`Context.Selection.Select(Entity{ InId, lWorld })`. Empty states are explicit, never a blank panel (L12):
no active world → `TextDisabled("No active world.")`; zero entities → `TextDisabled("World is empty.")`.
`Startup`/`OnPreRender`/`Shutdown` are no-ops.

### 2.6 `EditorService` (MODIFIED)

- Grows `UniquePtr<EditorSelection> m_Selection` and `TDynArray<UniquePtr<IEditorPanel>> m_Panels`
  (`ViewportPanel` stays its own named member — overview §3.3).
- New private `RegisterNativePanels()`: registers `"Hierarchy"` into `m_Extensions.Panels()`.
- `RegisterExtensions()` calls `RegisterNativePanels()` **before** `InCollect(m_Extensions)` and
  `Seal()` — mirroring D9/§2's "engine natives → game module → editor module → seal" ordering one level
  down, and matching the intent the existing comment in that function already records.
- `Initialize()`: build `m_Selection` before `m_Context`; after `m_ViewportPanel` (unchanged), iterate
  `m_Extensions.Panels().Entries()` in registration order, invoke each factory with `*m_Context`, call
  `Startup()`, push into `m_Panels`. Log the constructed count (L12 — the mechanism is otherwise
  invisible until a panel happens to draw).
- `BeginFrame()`/`EndFrame()`: one loop over `m_Panels` after the existing `m_ViewportPanel` call.
- `OnShutdown()`: `m_Panels` torn down in **reverse** construction order (LC3), positioned *between* the
  existing `m_ViewportPanel` step (stays first — its render-target ordering is load-bearing, M1) and the
  `m_UIBackend` step; `m_Selection.reset()` immediately before `m_Context.reset()`.

---

## 3. Ownership & lifetime (I5 / LC)

`EditorService` owns everything new: `m_Selection`, `m_Panels`, and (unchanged) `m_ViewportPanel`.
`EditorContext` holds *references*, never ownership — the flat-refs-injected-by-ctor shape from D3 that
M1 established for `UIBackend`. Panels never see the locator.

**Two-phase construction across two seams** (both already exist — §4):

```
OnModulesRegistered  → RegisterExtensions():  RegisterNativePanels() → game module → Seal()
   (pre-Engine::Startup, no EditorContext yet)   ... registry now holds {name, factory} pairs

PostEngineStartup    → Initialize():  m_Selection → m_Context → m_ViewportPanel
   (engine subsystems live)                     → for each registry entry: factory(*m_Context) + Startup()
```

**Per-frame:** `BeginFrame` — `m_ViewportPanel->OnPreRender()`, then the `m_Panels` `OnPreRender()` loop
(all no-ops in M2a) — all before `Engine().Loop()`. `EndFrame` — dockspace, `m_ViewportPanel->Draw()`,
then the `m_Panels` `Draw()` loop, then `ImGui::Render()`.

**Teardown (LC3):** `m_ViewportPanel` → `m_Panels` (reverse) → `m_UIBackend` → `m_Selection` →
`m_Context`. No panel in `m_Panels` holds a GPU resource, so the only ordering constraint is that they
die before the context they reference. Runtime (`Sandbox.exe`): none of this is linked — D4 holds
structurally.

---

## 4. Seam routing (SE) — **no new seam**

Reuses the two seams already carrying this traffic: **`OnModulesRegistered`** →
`EditorService::RegisterExtensions()` (grows the native-panel registration) and **`PostEngineStartup`** →
`EditorService::Initialize()` (grows selection + generic panel construction). `BeginFrame`/`EndFrame` are
already the `TickFrame` seam's body (M1) and each grow one loop. **No engine-side change whatsoever in
this slice** — `IEngine`, `Engine`, `OpaaxApplication` and every `Engine/Source` file are untouched.

---

## 5. Steps (each builds green; risk escalates)

- **S0 — Baseline.** *(Substantially done this session: `SandboxApp.cpp` unblocked, `debug-editor`
  `OPAAX_BUILD_OK`, **85 cases / 354 assertions / 2 skipped**.)* Remaining: confirm `release` and
  `release-editor` also build green before starting (the unblock touched `Sandbox/` code, which every
  preset compiles), and commit the unblock as its own change so M2a's diff stays clean.
- **S1 — `EditorSelection` + `EditorContext` growth.** New header; `EditorContext` gains `Selection`;
  `EditorService` gains `m_Selection`, builds it before `m_Context` in `Initialize()`, resets it in
  `OnShutdown()`. Nothing reads it yet. *Gate:* build green; `SandboxEditor.exe` boots identical to M1
  (same log lines); `Sandbox.exe` byte-identical.
- **S2 — `PanelRegistry` + generic panel collection + `HierarchyPanel` (first observable step).** New
  `PanelRegistry.h`; `EditorExtensionRegistrar::Panels()` returns it; new `HierarchyPanel`;
  `EditorService` grows `m_Panels`, `RegisterNativePanels()`, the construction loop, the two frame loops
  and the reverse teardown. **Also: remove the now-uncompilable `Panels().Register("Sandbox Panel",
  [](EditorContext&){ return 0; })` placeholder line from `SandboxEditorModule.cpp`** — a real panel
  replaces it in S3 (overview F1: the old placeholder cannot survive the registry going real; deleting
  one line here keeps S3's diff a pure dogfood proof). *Demo (observable, L12):* launch
  `SandboxEditor.exe` → a dockable **Hierarchy** panel lists `QuadRed`, `QuadGreen`, `QuadBlue`; clicking
  a row highlights it and keeps it highlighted; log shows `panels registered: 1, constructed: 1`.
  *Regression:* `Sandbox.exe` byte-identical.
- **S3 — Sandbox custom panel (milestone-delivering dogfood).** New
  `Sandbox/Editor/Source/SandboxEditor/Panels/SandboxPanel.{h,cpp}` — a real `IEditorPanel` showing the
  active world's entity count plus a "Spawn Quad" button that does
  `Worlds.GetActiveWorld()->CreateEntity("SpawnedQuad")` + `Add<DummyComponent>()` at an offset, logging
  `SandboxPanel: spawned quad #N`. `SandboxEditorModule::OnRegister` registers it:
  `Panels().Register("Sandbox Panel", [](EditorContext& c){ return MakeUnique<SandboxPanel>(c); })`.
  **This step's diff must touch only `Sandbox/Editor/**` — zero files under `Editor/Source/Editor/**`
  — the literal, checkable form of Editor.md's "zero changes to `OpaaxEditorLib`" gate.** *Demo:* a
  "Sandbox Panel" appears docked alongside Hierarchy/Viewport; clicking "Spawn Quad" adds a 4th quad
  visible in the Viewport, a new row in Hierarchy, and the log line. *Regression:* `Sandbox.exe`
  byte-identical (`SandboxModule`/`SandboxApp` untouched — the new files compile only into
  `SandboxEditor.exe`, per D9/D4's `Game/Editor` link rule).

---

## 6. Forks

Cross-slice forks (component choice, selection naming, `ViewportPanel` special-casing) are decided in the
overview §3 and not re-opened here. One local fork:

**Should `ViewportPanel` also be registered through `PanelRegistry`, for uniformity?** → **No.** It is
constructed directly, before the registry loop, and stays a named member. Registering it would mean its
construction order relative to `SetPrimaryRenderTarget` depends on registration order — the render target
must be set before the first `Render()`, and a game module registering panels ahead of it must not be
able to perturb that. Uniformity is not worth making a load-bearing ordering implicit (overview §3.3).

---

## 7. Verification

1. **3 presets** grep `OPAAX_BUILD_OK` (`debug-editor` / `release` / `release-editor`).
2. **Tests: unchanged at 85/354/2.** The editor lib is not linked into `Engine/Tests`, and this slice
   touches zero engine files — a delta here would mean something leaked out of the editor.
3. **`Sandbox.exe` byte-identical (D4)** at every step: 3 quads, 1280x720, 0 err/warn, no new log lines.
4. **`SandboxEditor.exe`** (observable, L12): S2 → Hierarchy lists the three named entities, click
   highlights, `panels registered/constructed` log; S3 → "Sandbox Panel" docked, "Spawn Quad" adds a
   visible quad + a Hierarchy row + `SandboxPanel: spawned quad #N`.
5. **S3's diff touches only `Sandbox/Editor/**`** — `git diff --stat` is the check.

---

## 8. Critical files

- Editor: `Editor/Source/Editor/EditorSelection.h` (new),
  `Editor/Source/Editor/Extensions/PanelRegistry.h` (new),
  `Editor/Source/Editor/Extensions/EditorExtensionRegistrar.h`,
  `Editor/Source/Editor/EditorContext.h`,
  `Editor/Source/Editor/Panels/HierarchyPanel.{h,cpp}` (new),
  `Editor/Source/Editor/EditorService.{h,cpp}`.
- Sandbox: `Sandbox/Editor/Source/SandboxEditor/Panels/SandboxPanel.{h,cpp}` (new),
  `Sandbox/Editor/Source/SandboxEditor/SandboxEditorModule.cpp` (S2 placeholder removal, S3 real
  registration).
- Engine: **none.**
- Reference only (X1, do not modify/depend): `Engine/Source/Legacy/Editor/Panels/HierarchyPanel.*`.
