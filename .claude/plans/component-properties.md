# Component properties — a default drawer, so two floats cost two lines

## Context

Every component that wants to be visible in the Inspector needs a hand-written drawer today:
a header, a `.cpp`, and a `Drawers().Register<TComponent, TDrawer>()` line
([DummyComponentDrawer.cpp](Sandbox/Editor/Source/SandboxEditor/Drawers/DummyComponentDrawer.cpp) is three
ImGui calls wrapped in ~25 lines of ceremony). For a component that is two floats that is far more work
than the component itself — which is why `HealthComponent` has **no drawer at all** and is invisible in
the Inspector today.

The goal is a **default** drawer generated from a per-type property list, with the hand-written drawer kept
as the **override** for fields that need judgment (`TagsComponentDrawer` is a tag picker — it stays).

Decided in discussion, and load-bearing for this plan:

- **Not editor-only.** [DummyComponent.h](Engine/Source/World/Components/DummyComponent.h) is compiled into
  the engine DLL (`OPAAX_WITH_EDITOR=0`, D4/I12) *and* into `SandboxEditor.exe` via `OpaaxEditorLib`
  (`=1`), so an `#if OPAAX_WITH_EDITOR` block inside that struct is one type with two definitions in one
  program. It also costs nothing to leave in: a `static constexpr` table nobody references is not emitted.
- **The macro emits DATA, never widgets.** The engine builds no ImGui (D4), so the property list is member
  pointers and names; the editor walks it.
- **Dispatch per field type is a trait specialization, not a registry** — `TPropertyDrawer<T>`, the same
  shape [TConfig.hpp](Engine/Source/Core/Config/TConfig.hpp)'s `TConfigCodec<TData>` already uses, whose own
  comment gives the reason: an undefined primary makes "you forgot to specialize" a compile error instead
  of a silent fallback. No registry means no static, no seal, no type identity across the DLL line.

## Engine — the property list (`Engine/Source/Core/Reflection/OpaaxProperty.h`, new)

Header-only, no `OPAAX_API` (stateless value templates — **I6**), no ImGui, no editor, no registry.

```cpp
struct DummyComponent
{
    Vector2F Position{0.f, 0.f};
    Vector2F Size{50.f, 50.f};
    Vector4F Color{1.f, 1.f, 1.f, 1.f};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DummyComponent, Position, Size, Color)

    OPAAX_PROPERTIES(DummyComponent,
        OPAAX_PROP(Position),
        OPAAX_PROP(Size),
        OPAAX_PROP(Color).SetHint(EPropertyHint::Color))
};
```

- `TProperty<TClass, TValue>` — `{ const char* Name; TValue TClass::* Member; EPropertyHint Hint; }` plus
  `ValueType`. `MakeProperty` deduces both types from the member pointer, so a field states its **name
  once** and never its type: adding support for a new field type is an editor-side overload, never a new
  `FLOAT_PROP`/`INT_PROP` macro in the engine.
- `OPAAX_PROPERTIES(Type, ...)` declares `using PropertyOwnerType = Type;` and
  `static constexpr auto GetProperties()` returning a `std::tuple` of them. Sits beside the NLOHMANN macro,
  same call-site shape the author already knows.
- `constexpr TProperty SetHint(EPropertyHint) const` returns a modified copy — facet chaining, the shape
  `EditorMenuCommandNode::SetEnabled/SetChecked/SetParams` already uses.
- `EPropertyHint { None, Color }` — **one value, one real caller**: `DummyComponent::Color` is a `Vector4F`
  that the hand-written drawer renders with `ColorEdit4`, and "a type is not always a widget" has to be
  expressible or deleting that drawer would be a visible regression.
- `concept CReflected` = `requires { T::GetProperties(); }` — optional and detected, no base class, the
  `CComponent`/`CResource` shape (**I8**).

## Editor — the generic drawer

**`Editor/Source/Editor/Properties/PropertyDrawer.h`** (new)
- `template<typename T> struct TPropertyDrawer;` — **declared, never defined.** A field type nobody
  specialized fails to compile at the `Register<T>()` line, naming the type. This is what makes the
  visibility rule safe: a specialization seen by one TU and not another cannot produce two different
  instantiations, it produces a build error.
- `DrawProperties<TComponent>(TComponent&)` — `std::apply` fold over `GetProperties()`, calling
  `TPropertyDrawer<ValueType>::Draw(Name, Component.*Member, Hint)`. Uniform 3-arg signature; only the
  `Vector4F` specialization reads the hint.

**`Editor/Source/Editor/Properties/PropertyDrawers.{h,cpp}`** (new) — the built-in specializations, ImGui
confined to the `.cpp`: `bool`, `Int32`, `Uint32`, `float`, `Vector2F`, `Vector3F`, `Vector4F` (`ColorEdit4`
under the Color hint, `DragFloat4` otherwise). Vectors go through `glm::value_ptr`, the idiom
`Renderer2D.cpp` and the current drawer already use.

**`OpaaxString` is deliberately NOT in that set.** It is the only one with real design cost — ImGui's
`InputText` needs a fixed buffer plus copy-back, since `imgui_stdlib.cpp` is not in the editor's ImGui
target — and no component in the tree has a string field. The scalars and vectors are one-liners with real
callers; the string waits for its first, and until then it is a compile error rather than a guess.

**`DrawerRegistry::Register<TComponent>()`** ([DrawerRegistry.h](Editor/Source/Editor/Extensions/DrawerRegistry.h))
— a one-argument overload beside the existing `Register<TComponent, TDrawer>()`. Same storage, same
`TryGet<TComponent>()` self-check, same `FDrawerInvoke` closure; it just calls `DrawProperties` instead of a
drawer, under a `CollapsingHeader` labelled with **`DeriveTypeLeafName<TComponent>()`**
([ModuleRegistrar.h:30](Engine/Source/Engine/Modules/ModuleRegistrar.h:30)) — reused rather than re-derived,
so the Inspector header and the component's key in a `.opaaxmap` are the same string by construction.
Logs once per registration: `Generic drawer: DummyComponent (3 properties)` — the count comes from
`std::tuple_size_v`, so it discriminates that the fold saw the right list.

## Dogfood — the gate

- `DummyComponent` and `HealthComponent` get `OPAAX_PROPERTIES`.
- **`DummyComponentDrawer.{h,cpp}` is deleted**, its registration becomes
  `Drawers().Register<Opaax::DummyComponent>()`.
- `HealthComponent` gets `Drawers().Register<Sandbox::HealthComponent>()` — a component that never had a
  drawer becomes visible and editable, which is the whole point of the slice in one line.
- **`TagsComponentDrawer` is untouched**, proving the override still works.

One accepted difference: the current drawer clamps `Size` to `1..4096` via `DragFloat2`'s min/max. The
generic drag has no clamp. A `.SetRange(min, max)` facet is ~4 lines when something needs it; nothing
enforces that range anywhere else, so it is not worth a second facet today.

## Verification

- **`Engine/Tests/Core/Reflection/PropertyTests.cpp`** (new) — a local struct with three fields:
  `static_assert` on `std::tuple_size` (proves `GetProperties()` is usable in constant evaluation), the
  names, read *and write* through `.*Member` landing in the right field, `CReflected` true for it and false
  for a plain struct, `SetHint` setting one property without touching its siblings. All engine-side, so it
  runs in `OpaaxTests` despite the known "tests cannot reach editor code" gap.
- `build.bat` on all three presets; verdict by grepping `OPAAX_BUILD_OK` (**L8**). Run `OpaaxTests.exe`
  directly.
- Editor smoke: back up `Sandbox/Editor/Save/imgui.ini` first, `ls -la` exe against DLL (**L24**), then
  confirm in `Sandbox/Save/Log/OpaaxEngine.log`: two `Generic drawer:` lines with counts 3 and 2,
  `drawers=3` at the seal, 0 err/warn. Restore the ini.
- **Interactive leg (yours):** select a quad — Position/Size drag it live, Color opens a picker, and
  `Health` now appears with two int fields where nothing was drawn before.
- Docs in the same change: Editor.md's `Drawers()` row (the two-form Register), and a new **I15** in
  ARCHITECTURE.md for the contract — properties are data in Core, dispatch is a trait specialization, a
  missing one is a compile error, the drawer remains the override.

## Explicitly NOT in this slice

- **Serialization from properties.** The NLOHMANN macro stays and the two lists coexist for now. Map files
  are byte-exact-verified (MP6 warns on a mismatch), so collapsing the two declarations is its own step with
  its own gate — not a side effect of a UI feature.
- **The Config panel using properties.** Next slice, and the reason `EngineConfigData` is untouched here.
- **A tag registry / tag dropdown.** That is I14's own named trigger with its home already decided
  (`EngineRegistries`, sealed, never a static). Because dispatch is a specialization, upgrading
  `TPropertyDrawer<OpaaxTag>` later touches one file and no component.
- **Enum properties** (need a value list C++20 cannot generate), **labels** and **ranges** (no caller),
  **`DrawDefaultProperties`** for partial override (no caller — `TagsComponent` has exactly one field).

---

## Review — LANDED 2026-08-19 (uncommitted)

Built as planned; no design deviations. `DummyComponentDrawer.{h,cpp}` deleted, `HealthComponent`
registered, `TagsComponentDrawer` untouched.

**Gates.**
- Tests **345 / 6554 / 7 skipped**, 0 failed (up from 341 / 6542 — four new cases). The `static_assert`s
  are the load-bearing half: they prove `GetProperties()` is usable in constant evaluation, which is what
  makes the table free in a shipped binary.
- **`OPAAX_BUILD_OK` on all three presets**, plus a `fast SandboxEditor` after the doc-comment fixes.
- Editor smoke: `Generic drawer: DummyComponent (3 properties)` / `Generic drawer: HealthComponent
  (2 properties)`, `drawers=3` at the seal, **0 err/warn**, 126 log lines. `imgui.ini` restored to its
  pre-run state byte-for-byte (the user had docked the Config panel since the last slice — that survived).

**What the smoke does NOT prove:** the ImGui calls themselves. A smoke run has no selection, so the fold
only ever gets instantiated, not executed — the count in the log discriminates that the property list was
read correctly, and nothing more. The widget leg is the interactive gate below.

**Still owed (interactive):** select a quad — Position/Size drag it live, Color opens a picker (not four
drag floats), and `Health` appears with two int fields where nothing was drawn before. Also worth a
glance: `Size` no longer clamps at 1..4096, the one accepted difference.

### Follow-up, same day — the crash the slice made easy to hit

The user added three floats to `DummyComponent` (properties **and** the NLOHMANN macro) and the app died
at boot: `out_of_range.403 key 'Size' not found`, out of `from_json` inside `Level::MountAll` →
`FinishStartup`, where nothing catches. `NLOHMANN_DEFINE_TYPE_INTRUSIVE` reads every field with `at()`,
which throws on a missing key — so the first field added to a component refuses **every map already
saved**. This slice's whole purpose was to make adding a field trivial, so it made a latent trap
routine to reach.

Fixed on both sides, and the two-sidedness is the point:
- **`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`** on all three components — an absent key keeps the
  default-constructed value, which makes "add a field" the backward-compatible change it looks like.
  `to_json` is unchanged, so what gets written is identical in shape.
- **`MapFactory::Instantiate` catches `nlohmann::json::exception`** around `Load` and warns per
  component, leaving it at its defaults. Defaults cannot cover a wrong-TYPED value or a payload that is
  not an object (hand-edited, truncated), and those must not kill a boot — **BO4c**'s rule one level
  down, and symmetric with the unknown-component-name branch already sitting above that line.

Pinned by two cases in `MapSnapshotTests.cpp`, **both written first and confirmed failing** with the
exact exceptions from the crash (`out_of_range.403`, `type_error.304`). Tests **347 / 6567**, three
presets `OPAAX_BUILD_OK`, and the editor now boots on the user's own `Main.opaaxmap` — which has no
`Test` keys — with `Instantiated 3/3`, `Generic drawer: DummyComponent (6 properties)`.

One expected warning remains: `'Maps/Main.opaaxmap' re-serializes DIFFERENTLY from disk` (**MP6**). That
is correct — the component now writes three more keys than the file holds. Saving the map once clears it.
→ task lesson (10 un-promoted): *when a slice makes an action cheap, price that action on the other
paths it touches.*
