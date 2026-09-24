# One serialization idiom, one drawer registry, and configs in the panel

## Context

Four corrections from the user, each of which makes the design smaller or more honest:

1. **A hint that says "draw this `Vector4F` as a colour" is a weaker version of a type.** A real
   `LinearColor` carries it, and dispatch goes back to being by type.
2. **…but hints are exactly right for BEHAVIOUR.** Some config values apply immediately and some need a
   restart, and only the author knows which. So the facet slot comes back as real metadata:
   `SetRange(min, max)` and `SetFlags(NeedRestart)`. The type says *what it is*; the facets say *how it
   behaves*.
3. **A colour is not maths.** It lives in `Core/Color/`, the way `OpaaxTag` lives in `Core/Tag/` and `Guid`
   in `Core/GUID/`.
4. **Two serialization idioms, and two drawer registries, are the annoyance.** A component uses
   `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` while a config uses hand-written `Parse`/`Serialize` + a
   codec specialization + a namespace of key constants; and a second, parallel drawer registry for configs
   would repeat that mistake one layer up. Both get unified — while the **call sites stay explicit**
   (`Drawers()` vs `ConfigDrawers()`), so a reader still sees which is which.

Format call (user): **unify now, nested structs**, so the JSON stays grouped and `EngineConfigData` mirrors
its own file. Both `.config` files are tracked, so regenerating them is a visible, revertible diff.

That choice has a consequence worth stating up front: **nested data means the property fold must recurse**,
since `TPropertyDrawer<WindowSettings>` will never exist. A field whose type is itself `CReflected` draws
as a nested group — and that is also what makes `NeedRestart` cheap to show, since it can sit on the
*group* rather than on all fourteen fields.

Three steps, each independently buildable and committable (**L17**).

---

## S1 — `LinearColor`, and a facet that carries metadata

**`Core/Color/LinearColor.h`** (new folder, one type) — a `Vector4F` with implicit conversions both ways,
so `Renderer2D::DrawQuad`, `RenderSystemDesc::ClearColor` and every other call site keep compiling
untouched. **`Core/Color/LinearColorJson.h`** holds the bridge, split the way `OpaaxTagJson.h` is split
from `OpaaxTag.h` — the renderer wants the colour, not nlohmann. Its json is **the vector's**
(`{x,y,z,w}`): the type exists for the editor and for call-site clarity, not to restate the file format,
and every saved `.opaaxmap` must keep its colours.

Named `LinearColor`, not `Color`, for a concrete reason: a member named `Color` of type `Color` shadows the
type inside its own struct — the trap `EditorContext::Route` already documents. So `DummyComponent::Color`
keeps its name and **map files do not change at all**.

**`Core/Reflection/OpaaxProperty.h`** — `EPropertyHint` is **deleted**; `SetRange(Min, Max)` takes its
place, with a real caller on day one: `DummyComponent::Size` gets back the `1..4096` clamp the components
slice dropped.

**`Editor/Properties/`** — `TPropertyDrawer<Vector4F>` loses its hint branch and is a plain `DragFloat4`;
the new `TPropertyDrawer<LinearColor>` is the `ColorEdit4`. The scalar/vector drawers honour `Range` when
set (`DragFloat`'s min/max).

*Gate: `git diff Sandbox/Assets/Maps/` is empty after a load+save cycle — nothing about the file moved —
and the Inspector still shows a picker for `Color`.*

---

## S2 — one way to serialize, engine-side only

**A generic `TConfigCodec`** ([TConfig.hpp](Engine/Source/Core/Config/TConfig.hpp)). Today the primary is
declared and never defined and every data type specializes it by hand; it becomes a *constrained* default —
`json(data).dump(4)` out, `json::parse` + `get_to` in — so any data type carrying the NLOHMANN macro
serializes with no codec of its own. "You forgot" stays a compile error (the constraint fails), and a type
needing a bespoke format can still specialize.

**Tolerance moves from every config into one place.** `ParseEngineConfig` is hand-written never to throw
(try/catch, `contains()` + `is_string()` per field); the macro is not. So `TConfig::Load` catches
`nlohmann::json::exception`, keeps the in-memory defaults and **returns false** — a value
`ConfigSystem::OnConfigRegistered` currently *discards* and will now log as a Warn. Core still does no
logging (**I11**); the Application layer does it.

**`EngineConfigData` becomes nested structs** mirroring the file: `WindowSettings Window`,
`AssetSettings Assets`, `LogSettings Log`, `RenderSettings Render`, `PhysicsSettings Physics` (holding
`WorldBoundsSettings WorldBounds`). Each carries the NLOHMANN macro **and** `OPAAX_PROPERTIES`.
`RendererConfigData` gets the same for its one `LinearColor ClearColor`.

**`EPropertyFlags` arrives here, because here is where it has callers** — a scoped bitmask
(`None`, `NeedRestart`) with a `constexpr operator|`, the shape `EEventCategory` uses after [[L4]], plus
`.SetFlags(...)` on `TProperty`. Every field of both configs is read once at boot today
(`RendererManager::Startup` copies `ClearColor` into a `RenderSystemDesc`; the window is built from its
config once), so the flag goes on the **group** properties — one marker on `Window`, not fourteen on its
fields. The day something is read live, its group loses the flag and the truth stays per-field rather than
living in a blanket sentence somebody has to remember to update.

**Deleted:** `ParseEngineConfig`/`SerializeEngineConfig` (165 lines), `RendererConfigData::Parse`/`Serialize`
(53), both `Opaax_Renderer_Config` key namespaces, `DECLARE_CONFIG_DATA`, `DECLARE_T_CONFIG_CODEC`, and the
`version` key — **written but never read** today (`lRoot[VERSION_KEY] = 1`, no field behind it). If
versioning is wanted it returns as a field with a reader.

**Three consumer sites**, all trivial: `MakeWindowProps`
([IWindowManager.cpp:53](Engine/Source/Application/Services/Window/IWindowManager.cpp:53)) →
`InData.Window.Title` etc., and `RenderBackend` → `.Render.Backend` in
[RendererManager.cpp:69](Engine/Source/Engine/Subsystems/Renderer/RendererManager.cpp:69) and
[WindowsWindow.cpp:69](Engine/Source/Platform/WindowsWindow.cpp:69). Nothing else reads a field of either
config — `Assets`, `Log` and `Physics` have **no reader at all** (CLAUDE.local already flags two); they
stay as the spec for systems not yet built, and the nesting makes that visible instead of hiding it in a
flat list.

**`Config_Renderer` must be exported** ([Config_Renderer.h](Engine/Source/Renderer/Config/Config_Renderer.h)):
`DECLARE_T_CONFIG` → `DECLARE_OPAAX_T_CONFIG`. `IMPL_T_CONFIG` defines its `StaticTypeID()` inside a DLL
`.cpp` and the class carries no `OPAAX_API`, so S3's editor-side `Register<Config_Renderer>()` is an
**LNK2019** — **I6**'s tell in mirror image, exported-ness only ever exercised from inside the DLL.
`Config_Engine` is already the exported shape and proves it links.

**Both `.config` files are regenerated** (delete + boot, commit the result). Keys become the C++ names, and
`worldBounds.min/max` become `{x,y}` objects rather than arrays — the hand-written serializer wrote those
one way while `clear_color` went through `MathsJson` the other, exactly the drift this step removes.
nlohmann sorts keys, so output stays byte-stable.

*Gate: `EngineConfigDataTests.cpp` rewritten against the new schema — full-schema read, missing fields keep
defaults, malformed json now proving `Load` returns **false** rather than throwing, and the round trip.
Both hosts boot with the right window size and backend; `Sandbox.exe` proves it without an editor.*

---

## S3 — one drawer registry, and configs in the panel

**`TDrawerRegistry<TSubject>`** replaces `DrawerRegistry`, with a resolver as the one customization point:

```cpp
template<typename TSubject, typename TTarget> struct TDrawerResolver;   // static TTarget* Resolve(TSubject&)
// <Entity,   T> -> InEntity.TryGet<T>()
// <IConfig,  T> -> InConfig.GetConfigTypeID() == T::StaticTypeID() ? static_cast<T*>(&InConfig) : nullptr
```

One registry, one entry shape (`TFunction<bool(TSubject&)>`), and **both forms available to both**:
`Register<TTarget, TDrawer>()` for hand-written UI, `Register<TTarget>()` for the generic fold over
`OPAAX_PROPERTIES`. A config gets the custom-drawer override for free, and "others" later is one more
resolver specialization rather than a third registry.

Explicit at the call site, as asked: `Drawers()` is `TDrawerRegistry<Entity>`, `ConfigDrawers()` is
`TDrawerRegistry<IConfig>`, with aliases so the names read plainly. `EditorExtensionRegistrar` gains the
7th route and `configDrawers=N` in the seal log; **`EditorService::RegisterNativeConfigDrawers()`**
registers `Config_Engine` + `Config_Renderer` beside the other `RegisterNative*` calls, natives before any
game module (**MR2**).

*Why the resolver, rather than keeping the config registry keyed by id: the entry shape then differs, and
two shapes is what the user objected to. The scan is over a handful of entries and only while a panel is
open — the Inspector already does exactly this for components.*

**The recursive fold** (`Editor/Properties/PropertyDrawer.h`): in `DrawProperty`,
`if constexpr (CReflected<ValueType>)` → `CollapsingHeader` + `DrawProperties(value)`, else the
`TPropertyDrawer<ValueType>` specialization. Without it a nested config cannot draw at all. The group's
header is where `NeedRestart` is shown.

**`TPropertyDrawer<OpaaxString>`** — deferred last slice for want of a caller; `EngineConfigData` is it.
Fixed 512-byte stack buffer copied in, `InputText`, copied back on change; a longer string draws
**read-only with a note** rather than silently truncating.

**[ConfigPanel](Editor/Source/Editor/Panels/ConfigPanel.cpp)** — the right pane draws the registered
drawer, or falls back to today's read-only `ToText()` view for a config nobody registered. Plus a **Save**
button enabled only when dirty, where dirty is *derived*: the panel keeps the config's `ToText()` from when
it last selected or saved it and compares each frame — the `EditorLevelDocument` shape one scale down, so
nothing has to remember to mark anything. `IConfig::Save()` already writes to the path it loaded from, so
**no new engine API**; a false return logs Warn. `ResolveCurrent()` returns a non-const `IConfig*`.

Per-field/per-group `NeedRestart` markers replace the blanket "changes apply on restart" line. The
one-shot log line names the path taken: `Showing 'Engine' (Engine.config, 5 properties)` vs
`… (no drawer — JSON view)`.

---

## Verification

- `OpaaxTests.exe` direct after each step; `build.bat` on all three presets per step, verdict by grepping
  `OPAAX_BUILD_OK` (**L8**). `Config_Renderer`'s export is proven by `debug`/`release` linking, not only by
  the editor preset.
- **S1's gate is that nothing moved** — an empty `git diff` on `Sandbox/Assets/Maps/` after load+save.
- **S2's gate is both hosts booting** on regenerated files: window title/size/backend all come from the
  config, so a mis-parsed schema shows up immediately.
- **S3:** smoke shows `configDrawers=2` and `Showing 'Engine' (…5 properties)`; then the interactive leg
  (yours, machine-checkable after): select `Renderer`, change the colour, Save, and
  `git diff Sandbox/Configs/Renderer.config` shows it. Restart to see the clear colour change. Back up
  `imgui.ini` around every editor run.
- Docs in the same change: **I15** gains the config half, the recursion rule and the resolver; Editor.md
  gets the `ConfigDrawers()` row; **I9**/**BO1** get a line about one codec and one tolerant reader.

## Explicitly NOT in this slice

- **Enum dropdowns.** `Window.Mode`, `Render.Backend`, `Log.Level` stay typo-able text fields; fixing that
  needs an enum value list C++20 cannot generate — the next slice, and the one that finally makes
  `WindowModeFromString`'s silent fallback unreachable from the UI.
- **Deleting the config fields nothing reads** (`Assets`, `Log`, `Physics`) — a spec, not dead code, and
  your call.
- **`ReadOnly` and other flags** beyond `NeedRestart`: the bitmask is built to grow, but each flag waits
  for something that needs it.
- **Revert**, and **live-applying** a changed config: `IConfig` has no `Reload()` and every consumer reads
  at boot. One method each when something wants them.

---

## Review — ALL THREE STEPS LANDED 2026-08-20 (uncommitted)

**S1 — `LinearColor` + facets.** One deviation, and it was an improvement: the type **derives** from
`Vector4F` rather than wrapping one. Two existing tests read `.a` / `.z` straight off the colour and
stopped compiling, which is the honest signal that a colour you cannot read `.r` from is a nuisance;
deriving also deleted both conversion operators. Tests 351/6581.

**S2 — one serialization idiom.** `EngineConfigData.cpp` (165) and `RendererConfigData.cpp` (53) are
**deleted outright**, along with two key-constant namespaces, `DECLARE_CONFIG_DATA`,
`DECLARE_T_CONFIG_CODEC` and the unread `version` key — 298 lines out of the config layer, 133 back in.
`OpaaxString` got the json bridge it never had. Both `.config` files regenerated in the nested schema;
`Sandbox.exe` boots from them with `Creating window Opaax Engine (1280, 720) [Windowed]`, which is the
gate — every one of those values came through the new reader. Tests 353/6593.

**A value nearly lost, and worth recording:** regenerating `Renderer.config` reset `ClearColor` to the
struct default (black), silently discarding the dark grey the user had set. Caught by reading the
regenerated file against `git show HEAD:` and carried across by hand. **Regenerating a tracked file
is a migration, not a rebuild** — diff it against what it replaced before moving on.

**S3 — one registry, configs in the panel.** `TDrawerRegistry<TSubject>` + `TDrawerResolver` replaced
the component-only registry; `DrawerEntry` disappeared into a plain `TFunction<bool(TSubject&)>`. The
recursion needed a forward declaration (`DrawProperty` and `DrawProperties` are mutually recursive now).

**Gates.** 3 presets `OPAAX_BUILD_OK` at every step. Editor smoke: `configDrawers=2`,
`Generic drawer: EngineConfigData (5 properties)` / `RendererConfigData (1 properties)`,
`Showing 'Engine' (Engine.config, properties)` — the drawer path, not the json fallback — and the only
warning is the pre-existing MP6 one about the user's `Test` fields. `imgui.ini` restored byte-for-byte
around every run.

**Repeat of a recorded mistake:** the "who reads this config field" grep covered `Engine/Source`,
`Editor/Source` and `Sandbox` and **omitted `Engine/Tests`** — `IWindowManagerTests` broke on
`lData.WindowTitle`. That is [[L10]] verbatim ("grep is a SEED, the build is the verdict… tests hardcode
what source does not"), same omission, same directory.

**Still owed (interactive):** open Window → Config, confirm the nested groups draw with `(restart)`
markers, edit the clear colour, press Save, then `git diff Sandbox/Configs/Renderer.config`.
