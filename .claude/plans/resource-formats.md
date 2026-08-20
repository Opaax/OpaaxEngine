# Resource formats — the engine owns extensions, the editor owns chrome

## Context

`EditorService::RegisterNativeResourceTypes` spells `".opaaxmap"` and `".opaaxlevel"` as raw literals
([EditorService.cpp:120](Editor/Source/Editor/Application/Services/EditorService.cpp:120),
[:131](Editor/Source/Editor/Application/Services/EditorService.cpp:131)) while the engine already owns both
as constants — `MAP_EXTENSION` ([MapFile.h:34](Engine/Source/World/Serialization/MapFile.h:34)),
`LEVEL_EXTENSION` ([LevelFile.h:61](Engine/Source/World/Serialization/LevelFile.h:61)). Two modules, one
spelling, nothing making them agree; **MR2c**'s "the panel id was interned twice" ([[L37]]) one folder over.

**The duplication is the symptom. The design fault is that a resource type has MANY extensions and the
editor's registry is keyed by ONE.** `TextureResource` claims `.png .jpg .jpeg .tga`, `SoundResource`
claims `.wav .mp3 .ogg` — each would need its own editor entry repeating the same icon and the same
double-click closure, and adding `.webp` would mean editing the editor. Flip the ownership and both tables
shrink:

| Table | Owner | Shape |
|---|---|---|
| extension → resource type | **engine** | many-to-one |
| resource type → icon, action, label override | **editor** | one-to-one |

A file resolves as extension → type (engine) → chrome (editor). Adding `.webp` becomes one line in the
engine and **nothing** in the editor.

**MR1a's premise has expired.** On 2026-07-27 the route dropped `Register<TAsset, TActions>()` because "the
engine has no such type" — true then. `LevelResource` and `MapResource` (**WM4**, 2026-08-03) satisfy
`CResource` now, so there is a type to bind, and many-to-one extensions are the reason to bind it.

## Engine — a format declares itself

**`Engine/Source/Engine/Subsystems/Resources/ResourceFormat.h`** (new), beside the `CResource` contract it
extends:

```cpp
struct ResourceFormat
{
    const char*        Label;           // "Opaax Level" — the format family's name
    const char* const* Extensions;
    Uint32             ExtensionCount;
};

#define OPAAX_RESOURCE_FORMAT(LABEL, ...)                                        \
    static constexpr const char*    Extensions[] = { __VA_ARGS__ };              \
    static constexpr ResourceFormat Format{ LABEL, Extensions,                   \
                                            sizeof(Extensions) / sizeof(*Extensions) };

template<typename T>
concept CResourceFormat = CResource<T>
    && requires { { T::Format } -> std::convertible_to<const ResourceFormat&>; };
```

In-class, the way `OPAAX_PROPERTIES` sits in a component — it is a facet beside `FailPolicy`, not a
declaration about the type from outside:

```cpp
struct LevelResource final
{
    OPAAX_RESOURCE_FORMAT("Opaax Level", LEVEL_EXTENSION)
    static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;
```

`MapResource` takes `("Opaax Map", MAP_EXTENSION)`. Both headers already include the file holding their
constant, so the extension is stated once in the whole tree.

**`const char*`, never `OpaaxStringID`.** A static data member holding an interned id reaches the string
pool during static init — the **I2** hazard that killed the out-of-line `String_None`. Interning happens at
`Register` time.

**Optional, and NOT folded into `CResource`.** `BinaryResource` is a format-agnostic byte blob with no
extension; requiring the facet would force it to invent one. The concept exists so `Register<T>()`
constrains on it — a type without the macro fails at the call site, naming `CResourceFormat`.

**I6 check, since it has bitten three times:** nothing new crosses the DLL line. `static constexpr` data
members are implicitly `inline` — compile-time data in a header, no linkage, nothing to export. The
registry stores `const ResourceFormat*`; when a **game** type registers, that points into exe data, which
outlives every use of it, and no address identity is required because lookups compare interned ids.

## Engine — `ResourceFormatRegistry`

**`Engine/Source/Engine/Subsystems/Resources/ResourceFormatRegistry.h`** — with its domain, not in
`Engine/Registries/`: `ComponentRegistry` lives in `World/Components/` and `WorldSubsystemRegistry` in
`World/Systems/`; `EngineRegistries.h` only aggregates.

```cpp
struct ResourceFormatEntry
{
    Uint32                TypeId;    // ResourceTypeID::Get<T>()
    OpaaxStringID         Name;      // authoring/log name, derived from the type by the route
    const ResourceFormat* Format;    // -> a static constexpr; process-lifetime
};

class OPAAX_API ResourceFormatRegistry
{
public:
    template<CResourceFormat T>
    bool Register(OpaaxStringID InName)          // required name — see ComponentRegistry's note
    {
        return AddEntry(ResourceTypeID::Get<T>(), InName, &T::Format);
    }

    void Seal() noexcept;

    const ResourceFormatEntry* FindByExtension(OpaaxStringID InExtension) const noexcept;
    const ResourceFormatEntry* FindByTypeId(Uint32 InTypeId)              const noexcept;

    const TDynArray<ResourceFormatEntry>& Entries() const noexcept;
    Uint64 Count()    const noexcept;
    bool   IsSealed() const noexcept;

private:
    bool AddEntry(Uint32 InTypeId, OpaaxStringID InName, const ResourceFormat* InFormat);  // DLL-side

    TDynArray<ResourceFormatEntry>    m_Entries;
    TUnorderedMap<Uint32, Uint64>     m_ByExtension;   // interned ext id -> INDEX, never a pointer
    bool                              m_bSealed = false;
};
```

**No type erasure, unlike both its siblings.** `IComponentEntry` / `IWorldSubsystemEntry` exist because the
engine *calls* through them; a format has nothing to call — it is data. The editor's own
`ResourceTypeRegistry` already made this call once ("a template would be ceremony with nothing to erase").

**`AddEntry` is out-of-line, DLL-side**, mirroring `ComponentRegistry`'s note: a game module instantiates
`Register<T>` but the entry list is only ever touched inside the DLL.

**`m_ByExtension` maps to an INDEX, not a pointer** — `m_Entries` is a `TDynArray` and reallocates. Same
discipline as the builder below.

**Duplicate extension → refused, Error naming both types.** Mirrors `ComponentRegistry::Register`, which
already refuses on sealed / invalid / already-taken and logs. A game *overriding* the engine's `.png`
loader is a legitimate future want, as an explicit `Override<T>()`, never a silent last-wins.

**`Register<T>()` calls `ResourceTypeID::Get<T>()` eagerly** — which is what gives the type its dense id
before anything loads it. Today those ids are minted lazily on the first `Load<T>`, so the engine cannot
enumerate its own resource types at all.

### Placement: `EngineRegistries`, not `ResourceManager`

The user's instinct was the manager. `EngineRegistries` wins on the axis this change is *for*:

- **A game defining its own resource type costs zero new wiring.** `ModuleRegistrar` is already bound to
  `EngineRegistries&`, and MR0's stated goal is that `BindEngineRegistries` *"does not grow an argument per
  registry."* On the manager, the registrar would need a **subsystem** pointer from a different owner —
  verbatim what MR0 says *"turns that subsystem into a bag"*, the mistake M3 already made by hanging
  `ComponentRegistry` off `WorldManager`.
- **It seals with the others.** A type registered after the first world would otherwise be silently missing
  from a world that already exists (**BO1a** shows the non-sealing registry needed its own argument).
- **Non-editor consumers** — a cooker, an importer, a validator — read the table without starting a
  subsystem, its pools, or the job system.

The manager's real advantage is that `LoadByPath("Sprites/hero.png")` will want extension→type one hop
sooner. That is a **borrow**, exactly as `WorldManager` borrows `EngineRegistries*`. **Nothing on
`ResourceManager` changes in this slice** — its frozen surface stays intact (*"editor type info … is a
separate system CONSUMING this API — never a manager feature"*).

### Wiring

- `EngineRegistries` gains `Resources()` + `m_ResourceFormats`, and one line in `SealAll()`.
- `ModuleRegistrar` gains `ResourceFormatRoute` — a copy of `ComponentRoute`'s shape including its
  unbound-route Error — plus `Resources()` and one line in `BindEngineRegistries`. The route derives the
  authoring name via `DeriveTypeLeafName<T>()` when omitted, which is why the registry itself takes a
  required name and needs no entt (`ComponentRegistry`'s stated reason).
- `Engine::RegisterNativeResourceFormats()`, called from the **Engine constructor** beside
  `RegisterNativeComponents()`, registers `LevelResource` and `MapResource`. That is `Bootstrap` time
  (**BO2**), so natives land before any module — **MR2**'s order, unchanged.

**Contract correction, same change (CLAUDE.md §0):** MR0 says the engine registers natives in
`Engine::RegisterNativeTypes()`. There is no such function — it is `RegisterNativeComponents()`
([Engine.cpp:63](Engine/Source/Engine/Engine.cpp:63)), called from the ctor, and this slice adds a sibling.

## Engine — the extension rule splits in two

`NormalizeExtension` is editor-only today ([ResourceScan.cpp:85](Editor/Source/Editor/Resources/ResourceScan.cpp:85))
and its own doc calls it *"The ONE place an extension becomes comparable."* Once the engine owns the
extension table there would be **two** normalizers — the exact drift this change exists to kill. It splits,
because the two halves have different contracts:

- **`PathString::Extension(OpaaxStringView) -> OpaaxStringView`** joins `Stem` in
  **`Core/String/OpaaxPathString.h`**: everything after the last dot, a leading dot being a dotfile with no
  extension. `constexpr`, byte-wise, returns a view into its argument — which is that header's stated
  contract, and why `NormalizeExtension` itself **cannot** go there (it lower-cases and interns, i.e. it
  allocates). Replaces the editor's anonymous `ExtensionOf` ([ResourceScan.cpp:16](Editor/Source/Editor/Resources/ResourceScan.cpp:16)).
- **`NormalizeExtension(OpaaxStringView) -> OpaaxStringID`** moves beside the registry (`ResourceFormat.h`,
  defined DLL-side): lower-case, guarantee the leading dot, intern. Registration and scanning both call it,
  so the two sides of the lookup cannot disagree.

The editor's copy is deleted; `ScanRoot` calls the engine's pair.

## Editor — chrome only

`ResourceTypeDesc` **loses `Extension`** and is keyed by type id instead:

```cpp
struct ResourceTypeDesc
{
    Uint32            TypeId;      // ResourceTypeID::Get<T>()
    OpaaxStringID     Label;       // OPTIONAL override; empty => the format's own Label
    OpaaxString       Icon;
    FResourceActivate OnActivate;
};
```

```cpp
InRegistrar.ResourceTypes().Register<LevelResource>()
    .SetIcon("[L]")
    .SetActivate([](EditorContext& InContext, const ResourceFile& InFile)
    {
        InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_OPEN_LEVEL_AT, InContext,
                                                LevelPathParams{InFile.AbsPath});
    });
```

Constrained on `CResourceFormat`, so chrome for a type with no format is a compile error. **No extension
appears anywhere in editor code.** One entry covers all four texture extensions.

**`ResourceTypeBuilder` holds `{ ResourceTypeRegistry*, Uint64 Index }`** and re-resolves per call — not a
`ResourceTypeDesc&`, because `m_Entries` is a `TDynArray` and the next `Register` reallocates. A returned
reference is safe only inside one full expression while the API permits otherwise: the
reference-into-a-vector failure class of the string-pool `Get` bug (**I2**). An index cannot dangle. A
refused registration (sealed, or a duplicate type) yields a sentinel index whose setters are no-ops, which
preserves today's "a bogus registration cannot make `Count()` lie".

`Register(ResourceTypeDesc)` is deleted — one grammar, not two ways to fill one struct.

**`ResourceBrowserPanel::FindType`** becomes two lookups: `m_Context.Engine.GetRegistries().Resources()
.FindByExtension(InFile.Extension)` → `TypeId` → the editor registry. `EditorContext` needs no new member —
it already holds `IEngine&`, and `GetRegistries()` is how `EditorService` reaches `WorldSubsystems()`
today. The displayed label is the override when set, else `Entry->Format->Label`.

**Naming — resolving the earlier open question at zero editor churn.** Engine `ResourceFormat` /
`ResourceFormatRegistry`, editor `ResourceTypeDesc` / `ResourceTypeRegistry` unrenamed. A *format* is what
a file is on disk and naturally carries many extensions; a *resource type* is the editor's browsable kind.
Two words, two things, no editor file renamed.

## Sandbox — the first game-module resource type

`.wave` has no C++ type, and under this model a file the engine cannot load is not a resource type — it is
just a file. `Sandbox/Source/Sandbox/Resources/WaveResource.hpp` (new, ~15 lines): `Load` reading the file
through `BinaryResource`'s idiom, `Placeholder`, `FailPolicy = Placeholder`,
`OPAAX_RESOURCE_FORMAT("Wave Definition", ".wave")`. Registered in `SandboxModule::OnRegister` via
`InRegistrar.Resources().Register<WaveResource>()`; the editor module keeps only its icon and its
activation log.

That is **the point of the slice as much as the dedup**: the first game-module resource type, exercising a
D9 route that has never been run, and the proof that a game extends the format table without touching the
engine.

## Order of work

1. `ResourceFormat.h` — POD, macro, concept, `NormalizeExtension`.
2. `PathString::Extension`; editor `ExtensionOf`/`NormalizeExtension` deleted, `ScanRoot` repointed.
3. `ResourceFormatRegistry` (+ `.cpp`); `EngineRegistries::Resources()` + `SealAll`.
4. `ResourceFormatRoute` + `ModuleRegistrar::Resources()` + `BindEngineRegistries`.
5. `OPAAX_RESOURCE_FORMAT` on `LevelResource`/`MapResource`; `Engine::RegisterNativeResourceFormats()`.
6. Editor: desc keyed by type id, `Register<T>()` + builder, browser two-step lookup.
7. Sandbox `WaveResource` + both module registrations; Creator template comment.
8. Docs: Editor.md D10 (:275, :290, :302), ARCHITECTURE MR1a, MR0's registry list + its
   `RegisterNativeTypes` correction.

Steps 1–5 are engine-only and land green on their own — the editor is untouched until 6, so a break in the
first half cannot be confused with a break in the second.

## Verification

- **`Engine/Tests/Core/Engine/Subsystems/Resources/ResourceFormatRegistryTests.cpp`** (new), modelled on
  `ComponentRegistryTests.cpp`: a registered type is found by **each** of its extensions and by its type
  id; lookup is case- and dot-insensitive (`"PNG"`, `".PNG"`, `".png"` all hit); an unregistered extension
  answers null; a **duplicate extension across two types is refused and logged, and the first registration
  survives**; registering after `Seal()` is refused; `Count()` is types, not extensions; two structurally
  identical formats get distinct type ids.
- **`Engine/Tests/Core/PathStringTests.cpp`** gains `Extension`: `"Maps/Decor.opaaxmap"` → `".opaaxmap"`,
  `"No.Dots.Here.opaaxmap"` → `".opaaxmap"`, `".gitignore"` → empty (dotfile), `"C:/a.b/Maps/Decor"` →
  empty (a dot in a *directory* does not count — the case `Stem` already pins), `"Decor."` → `"."`.
  `static_assert` where `Stem`'s cases already are, since it is `constexpr`.
- **`Engine/Tests/Core/Engine/ModuleRegistrarTests.cpp`** gains the unbound-route case for `Resources()`,
  matching the two that exist.
- `build.bat` on all three presets, verdict by `OPAAX_BUILD_OK` (**L8**); `OpaaxTests.exe` run direct.
- **THE GATE — the duplication is mechanically gone:** `rg -n '"\.opaaxmap"|"\.opaaxlevel"'` over live
  source hits **only** `MapFile.h:34` and `LevelFile.h:61`, and `rg -n '"\.[a-z]+"' Editor/Source` returns
  nothing format-shaped. That is the whole claim, checkable without running anything.
- Boot `Sandbox.exe`: the startup level still resolves (`Load<LevelResource>` is unchanged) and
  `git status Sandbox/` is empty — no format, no map and no config moved.
- Editor smoke (**back up `imgui.ini` first** — a run flushes the tiny window's geometry into the user's
  layout, which has cost three sessions): the seal log gains the format count; the Resource Browser still
  shows `[M] Opaax Map`, `[L] Opaax Level`, `[W] Wave Definition`; double-click opens a `.opaaxlevel` and a
  `.opaaxmap`, and `.wave` still logs. **0 err/warn.**
- Then the interactive leg (yours): rename a `.opaaxmap` to `.OPAAXMAP` on disk and confirm the browser
  still shows it as an Opaax Map — the case-insensitivity claim, which no unit test can make about the real
  scanner.
- Docs in the same change: **MR1a** (its own test survives — the route stayed descriptor-shaped and the
  payload arrived as a *value*; what expired is "the engine has no such type"), **MR0** (third registry,
  plus the `RegisterNativeTypes` → `RegisterNativeComponents` correction), Editor.md **D10**.

## Explicitly NOT in this slice

- **`LoadByPath` / extension-driven loading.** The table makes it possible; nothing asks. **Trigger: the
  first component field that references a resource** — the same trigger Editor.md:302 names for the GUID
  catalog, and the point at which `ResourceManager` borrows the registry.
- **A type table on `ResourceManager`.** See the placement argument. Revisit with `LoadByPath`, as a
  borrow.
- **`Override<T>()` for duplicate extensions.** Refuse + Error until a real case appears. **Trigger:** a
  game wanting its own loader for an engine-claimed extension.
- **`TextureResource` / `SoundResource` themselves.** This slice builds the table they will use; the
  loaders belong to the sprite slice. The macro's multi-extension form is exercised by a test, not by a
  live type — the one thing here without a production caller, and deliberately so, because designing the
  table around a single extension is precisely the mistake being corrected.
- **An extension assert at the two `Load<T>` sites.** A wrong extension already fails inside
  `MapFile`/`LevelFile` with a named warning; the assert would restate it one layer up.
- **Renaming the editor's `ResourceType*` pair.** The `ResourceFormat` naming makes the collision moot.

---

## Review — LANDED 2026-08-20, NOT committed. Tests **374 / 6672 / 7**, all three presets `OPAAX_BUILD_OK`

Built as planned; no design deviations. The two decompositions the plan added after reading the code
(`PathString::Extension` split from `NormalizeExtension`; no type erasure in the registry) both held.

**One thing the plan did not predict, and it is the most useful finding here.** The `.wave` registration
came up dead on the first `Sandbox.exe` boot:

```
[error] [ModuleRegistrar] Resources().Register — route is not bound to a ResourceFormatRegistry; registration dropped.
```

`OpaaxApplication::PopulateEngineRegistries` bound each route **by hand** —
`Components().Bind(...)`, `WorldSubsystems().Bind(...)` — while `ModuleRegistrar::BindEngineRegistries`,
the one call **MR0** built so the binding *"does not grow an argument per registry"*, sat there with
**zero callers**. So a new route ships unbound by default, and the only reason this was a one-line fix
rather than a silent content bug is that the route logs its own refusal ([[L16]]'s shape: the unbound
Error earns its keep the first time somebody adds a route). Fixed at the cause — `PopulateEngineRegistries`
is now that one call — so the *next* registry costs nothing there. → **MR0** corrected.

**A second doc-vs-code correction fell out of the same area:** MR0 said the engine registers natives in
`Engine::RegisterNativeTypes()`. No such function has ever existed; it is `RegisterNativeComponents()`,
called from the **constructor**, and this slice added `RegisterNativeResourceFormats()` beside it.

**Gates.**
- `OPAAX_BUILD_OK` on `debug`, `debug-editor`, `release`.
- Tests **374 / 6672**, 0 failed — exactly the 17 cases added (was 357 / 6602).
- **THE GATE:** `rg -n '"\.opaaxmap"|"\.opaaxlevel"'` over live source hits **only** `MapFile.h:61` and
  `LevelFile.h:34` — plus one doc comment in `OpaaxPathString.h`. No format-shaped literal survives in
  `Editor/Source` (the remaining `.opaaxproj` is a project file, not a resource format).
- `Sandbox.exe` boot: 3 formats over 3 extensions, `WaveResource` among them **from the game module**,
  0 err/warn, and `git status Sandbox/` shows only my own source edits — no config, map, level or layout
  moved.
- `SandboxEditor.exe` boot: same 3 formats, `resourceTypes=3` (the editor's chrome count, unchanged),
  **0 err/warn**, and `imgui.ini` was byte-identical after the run.

**Interactive gate: MET, by the user.** A file renamed to `.OPAAXMAP` still reads as an Opaax Map in the
browser — which exercises the whole chain end to end and is the one claim no unit test can make: the real
scanner's `PathString::Extension` + `NormalizeExtension` agreeing byte-for-byte with what the registry
interned at registration, then the two-step lookup, then the label falling back to the engine's format.
That is exactly the risk named as this slice's riskiest step (the extension rule moving modules).

**Named, not fixed:** the multi-extension form — the reason for the whole design — still has **no
production caller**. `ProbeTextureResource` in the tests is the only type claiming more than one
extension. That was the plan's stated bet; it comes due with the sprite slice.
