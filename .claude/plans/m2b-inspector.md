# Plan — Editor M2b "Drawers + Inspector"

> **Provenance:** second slice of the M2 program — see `.claude/plans/m2-panels.md` for the
> decomposition, the dependency graph and the cross-slice decisions (not repeated here). Base:
> `875bc85` (M2a complete). Structure follows the M1/M2a precedent. Approved 2026-07-27.
> Live tracking → `.claude/task/todo.md`.

## Context

M2a landed the panel infrastructure: panels are `{name, factory}` in a `PanelRegistry`, built by one
generic loop, and `EditorSelection` is written by `HierarchyPanel`. **Nothing reads that selection yet** —
clicking a row highlights it and logs, and that is all.

M2b closes Editor.md's actual M2 gate: *"click-select + live transform edit; a Sandbox-module custom
drawer AND custom panel appear with zero changes to `OpaaxEditorLib`."* The panel half shipped in M2a
(`SandboxPanel`). This slice ships the drawer half — the second registry graduates from counts-only to
real storage, an `InspectorPanel` consumes it, and the Sandbox module supplies a real drawer.

Second of four M2 slices; M2c/M2d remain independent after this. Overview + cross-slice decisions:
`.claude/plans/m2-panels.md`. **Touches zero engine files**, like M2a.

---

## 1. What lands

### 1.1 `DrawerRegistry` (NEW — `Editor/Source/Editor/Extensions/DrawerRegistry.h`)

Replaces `EditorRoute` behind `EditorExtensionRegistrar::Drawers()`. The call site is **unchanged** from
the M0 skeleton (`Drawers().Register<TComponent, TDrawer>()`) — only the body becomes real, which is what
MR1 / the `EditorRoute` header comment promise.

```cpp
using FDrawerInvoke = TFunction<bool(Entity&)>;      // true = the component was present and drawn

struct DrawerEntry { FDrawerInvoke Invoke; };

template<typename TComponent, typename TDrawer>
void Register();                                     // stores a closure; constructs nothing
const TDynArray<DrawerEntry>& Entries() const noexcept;
Uint64                        Count()   const noexcept;   // keeps the existing seal log compiling
```

The stored closure is the whole design — it erases both types into one uniform call:

```cpp
[](Entity& InEntity) -> bool
{
    TComponent* lComp = InEntity.TryGet<TComponent>();
    if (lComp == nullptr) { return false; }
    TDrawer lDrawer;              // duck-typed contract: default-constructible,
    lDrawer.Draw(*lComp);         // with one method  void Draw(TComponent&)
    return true;
};
```

This is why the Inspector needs **no** component reflection and no entt introspection: it does not ask
"what components does this entity have?", it asks every registered drawer "are you applicable?" and each
one self-checks via `Entity::TryGet<T>()`. Registration order is display order.

**No `IComponentDrawer` base class** — D7 declines OOP/virtual component drawers; the contract is
duck-typed and checked at instantiation.

Invariants: **I1** no static · **I5** owned by `EditorExtensionRegistrar` (a by-value `EditorService`
member) · **I2** n/a (`OpaaxEditorLib` is a static lib linked wholly into `SandboxEditor.exe`; nothing
here crosses the engine DLL boundary) · **I6** `DrawerRegistry` is a plain class with a member template,
never a dll-exported class template.

### 1.2 `EditorExtensionRegistrar` (MODIFIED)

`Drawers()` returns `DrawerRegistry&`. `AssetTypes()`, `Menus()`, `EditWorldSystems()` stay `EditorRoute`
(M2d / M5 / M4). `Seal()`/`IsSealed()` and the five-count seal log are untouched — `Count()` keeps its
signature, exactly as `PanelRegistry` did in M2a.

### 1.3 `EditorContext` (MODIFIED)

Grows one member, as M2a's plan foreshadowed:

```cpp
const EditorExtensionRegistrar& Extensions;   // sealed before the context is built; panels read routes
```

`const` is honest: sealing happens at `OnModulesRegistered`, the context is built at `PostEngineStartup`,
so by construction nothing can register through this reference. One member serves M2d's `AssetTypes()`
too — the alternative (a separate `const DrawerRegistry&`, then `const AssetTypeRegistry&`, …) grows a
member per slice, all aimed at the same object.

### 1.4 `InspectorPanel` (NEW — `Editor/Source/Editor/Panels/InspectorPanel.{h,cpp}`)

A native `IEditorPanel`, registered in `EditorService::RegisterNativePanels()` **through the same
`PanelRegistry` route** a game panel uses — the M2a mechanism, now with a second customer.

`Draw()`: no selection → `TextDisabled("Nothing selected.")`. Otherwise the entity's name (from
`EntityMeta`; the `Guid` is a bare `{High, Low}` pair with no string conversion, and inventing a Guid
formatter is out of scope for a panels milestone), a separator, then every registry entry invoked in
order. If no entry returned true → `TextDisabled("No drawable components.")` — an explicit empty state,
never a blank panel (L12), and the reason `FDrawerInvoke` returns `bool` rather than `void`.

`Startup`/`OnPreRender`/`Shutdown` are no-ops.

### 1.5 Sandbox `DummyComponentDrawer` (NEW — `Sandbox/Editor/Source/SandboxEditor/Drawers/`)

`void Draw(DummyComponent&) const` — `DragFloat2` Position, `DragFloat2` Size, `ColorEdit4` Color, under
a `CollapsingHeader`, writing straight into the live component. Bound with `glm::value_ptr` (the
codebase's idiom — `Renderer2D.cpp:186`, `OpenGLShader.cpp:236`); `Vector2F`/`Vector4F` are `glm::vec2`/
`vec4`.

Declared in the header, **defined in the `.cpp`** so `<imgui.h>` stays out of `SandboxEditorModule.cpp`.
That works because the registry's closure only needs to *call* `TDrawer::Draw`, which resolves at link
time — the duck-typed contract does not require an inline body.

Per overview §3.1 the Inspector edits `DummyComponent`, the only component `RendererManager::Render`
actually reads (`RendererManager.cpp:129` → `Renderer2D::DrawQuad`). A new `TransformComponent` would
either duplicate data nothing draws or force a render-path change inside a panels milestone (L3).
`Register<TComponent, TDrawer>()` is a template, so swapping the component in M3 is a one-line change.

---

## 2. Verified up front (not assumed — L16)

**The M0 placeholder `Drawers().Register<int, int>()` cannot survive this change.** Type-checked against
the template body above: `int::TryGet` does not exist, and `TDrawer lDrawer; lDrawer.Draw(...)` is
ill-formed for `int`. `Register<int,int>()` *is* an instantiation, so this is a hard compile error, not a
latent one. Consequence: the placeholder must be removed in the same step the registry goes real — the
same atomicity that forced the four-slice split. Handled in S1.

**Live edits are safe from `Draw()`.** `EditorApplication::TickFrame` runs `BeginFrame` → `Engine().Loop()`
→ `EndFrame`, and panels draw in `EndFrame`. So an edit lands *after* the world render for that frame,
with no render iteration and no Hierarchy `Each<EntityMeta>` in flight. The quad moves on the next frame
(~16ms at 60fps with VSync on — imperceptible during a drag; not worth designing around).

---

## 3. Steps (each builds green)

**S1 — `DrawerRegistry` + `InspectorPanel` (first observable).**
`DrawerRegistry.h`; `EditorExtensionRegistrar::Drawers()` returns it; `EditorContext` grows `Extensions`;
`InspectorPanel.{h,cpp}`; `RegisterNativePanels()` registers `"Inspector"`; **remove**
`Drawers().Register<int, int>()` from `SandboxEditorModule.cpp` (§2 — it cannot compile).
*Gate:* `SandboxEditor.exe` shows a dockable Inspector; nothing selected → "Nothing selected."; click a
Hierarchy row → the entity's name + "No drawable components." (honest: no drawer is registered yet). Seal
log reads `drawers=0`, `panels=3`, `constructed: 3`.

**S2 — Sandbox drawer (milestone-delivering dogfood).**
`Drawers/DummyComponentDrawer.{h,cpp}` + `Drawers().Register<DummyComponent, DummyComponentDrawer>()`.
**This step's diff must touch only `Sandbox/Editor/**`** — the checkable form of "zero changes to
`OpaaxEditorLib`", as M2a's S3 was.
*Gate:* select `QuadRed` → Inspector shows Position / Size / Color → drag Position → the red quad moves in
the Viewport; `ColorEdit4` recolors it. Seal log back to `drawers=1`.

---

## 4. Forks (decided, not re-opened)

- **Hierarchy → Inspector travels the CONTEXT, not an editor event** (user decision, 2026-07-27).
  `InspectorPanel::Draw()` runs every frame and renders whatever `Selection.Get()` returns right now —
  ImGui is immediate-mode, so there is no "on change" work for an event to trigger. A notification would
  be stored in a member and then read during `Draw()`, i.e. what `Get()` already does, plus indirection
  and a real cost: `TMulticastDelegate::Add` returns a handle the listener **must** `Remove` before it
  dies, giving every panel an unbind obligation.
  *For the record, the premise it was raised on:* an editor bus would **not** copy `EngineEventBus`.
  `Core/Events/EventBus.h` is already generic and DLL-safe (`__FUNCSIG__`-keyed); `EngineEventBus` is
  just its subsystem wrapper. An editor bus = `EventBus m_Events;` owned by `EditorService`. Cheap to
  add — but it earns its place only with real subscribers, all of which are M4+: PIE state across
  panels, asset-import → browser refresh (M2d), scene dirty/saved (M5). Building it here would be an
  API with no caller, the same thing F3 already ruled out for DebugDraw.
  *When it returns:* prefer the cheapest tier that works — a Tier-2 delegate on the producer
  (`WorldManager`'s shape) before a Tier-3 bus. Note the `EditorSelection` M4 FIXME is satisfied by
  subscribing to the engine's **existing** `WorldManager::OnWorldDestroyed`, not by a new editor bus.
- **`const EditorExtensionRegistrar&` in the context**, not a per-route reference — §1.3.
- **Drawer owns its own presentation** (its own `CollapsingHeader`), so `Register<TComponent, TDrawer>()`
  needs no display-name argument and the M0 call-site shape survives byte-for-byte (MR1).
- **Edit `DummyComponent`**, not a new transform type — overview §3.1.
- **Inspector is a native panel registered through `PanelRegistry`**, not a named member: unlike
  `ViewportPanel` it owns no GPU resource and no ordering is load-bearing, so the M2a special-case
  reasoning (overview §3.3) does not apply to it.

---

## 5. Verification

1. **3 presets** grep `OPAAX_BUILD_OK` (`debug-editor` / `release` / `release-editor`).
2. **Tests unchanged at 85 / 354 / 2.** Zero engine files touched, and `OpaaxEditorLib` is not linked into
   `Engine/Tests` — so `DrawerRegistry` is not unit-testable from that suite and adds no case. A delta
   here means something leaked out of the editor.
3. **`Sandbox.exe` unchanged (D4)** at both steps: 58 log lines, 3 entities, 1280x720, 0 err/warn, and 0
   editor/imgui/panel mentions.
4. **`SandboxEditor.exe`** observable gate per step (§3), each pointing at specific UI + the seal-log
   counts — never "no errors" alone (L12).
5. **S2's diff touches only `Sandbox/Editor/**`** — changed + untracked paths filtered for anything
   outside it must come back empty, the check used for M2a's S3.
6. Graceful window close → clean reverse teardown, 0 err/warn (the Inspector joins `m_Panels`, so it is
   torn down in the existing reverse loop; nothing new to order).

---

## 6. Critical files

- **New:** `Editor/Source/Editor/Extensions/DrawerRegistry.h`,
  `Editor/Source/Editor/Panels/InspectorPanel.{h,cpp}`,
  `Sandbox/Editor/Source/SandboxEditor/Drawers/DummyComponentDrawer.{h,cpp}`.
- **Modified:** `Editor/Source/Editor/Extensions/EditorExtensionRegistrar.h`,
  `Editor/Source/Editor/EditorContext.h`, `Editor/Source/Editor/EditorService.cpp`
  (`RegisterNativePanels` only), `Sandbox/Editor/Source/SandboxEditor/SandboxEditorModule.cpp`.
- **Engine: none.** CMake: none — both trees `GLOB_RECURSE ... CONFIGURE_DEPENDS`.
- **Reference only (X1 — do not depend on):** `Engine/Source/Legacy/Editor/` `IComponentDrawer`
  (virtual-dispatch shape D7 declines) and the old Inspector panel (layout ideas only).
