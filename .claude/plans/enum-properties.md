# Enum properties — a dropdown, and the end of stringly-typed config

## Context

`Window.Mode`, `Render.Backend`, `Log.Level` are `OpaaxString` in
[EngineConfigData.h](Engine/Source/Engine/Config/EngineConfigData.h), so the Config panel offers a free
text field you can typo, and `WindowModeFromString` answers a typo by silently falling back to Windowed.
The dropdown is the fix, and it only exists if the field is a **real enum** — which is the actual
deliverable: the mechanism has no caller until a field has a type.

C++20 cannot enumerate an enum's values, so this needs one declaration per enum. **I11** already gives
every engine enum a free `ToString(E)` found by ADL, so half the table exists; the missing half is the
list of values, and that same list makes the *parse* direction fall out for free.

Two enums have real callers today and both are already written: `EWindowMode`
([Core/Window/Window.h:17](Engine/Source/Core/Window/Window.h:17)) and `EBackend`
([RHI/RHIBackend.h](Engine/Source/RHI/RHIBackend.h)). Their config fields become those types.

**The file does not change.** `to_json` writes `ToString(value)`, which is the same `"Windowed"` /
`"OpenGL"` already on disk — so `git diff Sandbox/Configs/` staying empty after a boot is this slice's
sharpest gate.

## Engine — the mechanism

**`Core/Reflection/OpaaxEnum.h`** (new) — `TEnumValues<E>`, declared and never defined, specialized by
one macro; the `TConfigCodec` / `TPropertyDrawer` idiom a third time, so an enum nobody declared values
for is a compile error rather than an empty dropdown.

```cpp
enum class EWindowMode { Windowed, Borderless, Fullscreen };
OPAAX_ENUM_VALUES(EWindowMode, Windowed, Borderless, Fullscreen)
```

The macro emits `using enum EnumType;` inside the specialization (C++20 P1099), which is what lets the
call site name enumerators **unqualified** — the list reads like the enum it follows. It also carries the
qualified-`::Opaax::` specialization shape the deleted `DECLARE_T_CONFIG_CODEC` proved compiles here.
Concept: `CEnumWithValues<T>` = an enum, with `TEnumValues<T>::Values`, and a `ToString` reachable by ADL.

**`Core/Reflection/OpaaxEnumJson.h`** (new) — a generic `to_json`/`from_json` for any
`CEnumWithValues`, split from the type the way every other json bridge here is. It writes `ToString(v)`
and reads by matching the label against the value list, which is why **no per-enum `FromString` is
needed**: the list is the parser.

An unknown label **throws**, and that is deliberate consistency rather than a new policy — post-`BO1b` a
wrong-typed value anywhere in a config already throws, `TConfig::Load` catches it, keeps the defaults and
returns false, and `ConfigSystem` warns naming the file. A typo'd enum is now the same event as a typo'd
number, and the dropdown means the editor cannot produce one.

## Engine — the two conversions, and a function that was doing two jobs

**`ToString(EWindowMode)` moves to `Core/Window/Window.h`**, beside its enum, with the values list.
**I11** put it in `IWindowManager.h` *because "an unknown mode must be loud and Core does not log"* — but
that reason belongs to `WindowModeFromString`, which logs; `ToString` is total and silent and was only
carried along. With the parse generic, the exception evaporates and I11's own default rule applies.

**`WindowModeFromString` is deleted** — its one production caller is `MakeWindowProps`
([IWindowManager.cpp:53](Engine/Source/Application/Services/Window/IWindowManager.cpp:53)), which now
passes `InData.Window.Mode` straight through.

**`BackendFromString` splits, because it is two things wearing one name.** It parses a string *and* it
carries a policy: `Vulkan` → OpenGL with a Warn, since the VK backend is parked in Legacy. The parse dies
with the string field; the policy survives as **`ResolveSupportedBackend(EBackend)`** in the same
`BackendFactory.cpp`, keeping its log. Its two callers — [RendererManager.cpp:69](Engine/Source/Engine/Subsystems/Renderer/RendererManager.cpp:69)
and [WindowsWindow.cpp:69](Engine/Source/Platform/WindowsWindow.cpp:69) — wrap the config value in it
instead of parsing.

**`EngineConfigData`**: `WindowSettings::Mode` becomes `EWindowMode`, `RenderSettings::Backend` becomes
`EBackend`. Both keep `OPAAX_PROP`, and both keep their `NeedRestart` group flag.

**Not converted, and each for a stated reason:** `Log.Level` — `ELogLevel` has no `ToString`, its file
value is lowercase (`"trace"`) where enumerators are `Trace`, so converting would change the file for a
field **nothing reads**. `Physics.Backend` / `WorldBounds.Response` — no enum type exists; physics is not
built, and those strings are the spec for it.

## Editor — the dropdown

**`Editor/Properties/PropertyDrawers.h`** gains ONE constrained partial specialization covering every
enum at once:

```cpp
template<CEnumWithValues T> struct TPropertyDrawer<T>   // BeginCombo over TEnumValues<T>::Values
```

Body in the header, since it is a template — `PropertyDrawer.h` already includes `<imgui.h>`. The preview
is `ToString(current)`, each row is `ToString(value)`, and selection assigns. Nothing per-enum, and a new
enum field anywhere gets a dropdown from its `OPAAX_ENUM_VALUES` line alone.

## Verification

- **`Engine/Tests/Core/Reflection/EnumTests.cpp`** (new): the values list is a constant expression and
  has the right size; every value round-trips enum → json → enum; the json IS the `ToString` label (a
  string, not an int — the claim the file format rests on); an unknown label throws; a type that declared
  no values fails `CEnumWithValues`.
- `IWindowManagerTests.cpp` loses the `WindowModeFromString` cases (the function is gone) and keeps
  `MakeWindowProps`, now handed an `EWindowMode` directly. `EngineConfigDataTests.cpp` gains: `"Mode":
  "Borderless"` parses to the enumerator, and an unknown mode makes `TConfig::Load` answer false with the
  defaults intact.
- `build.bat` on all three presets, verdict by `OPAAX_BUILD_OK` (**L8**); `OpaaxTests.exe` direct.
- **THE GATE: `git diff Sandbox/Configs/` is EMPTY after booting `Sandbox.exe`.** The enum writes the
  same label the string held, so a byte of movement means the round trip is wrong. Boot also proves the
  read: the window is `1280x720 [Windowed]` and the backend resolves to OpenGL, both now from enums.
- Editor smoke: `configDrawers=2` and `Showing 'Engine' …` unchanged; then the interactive leg (yours) —
  Window → Config, `Mode` and `Backend` are **combos**, and picking `Borderless` + Save writes
  `"Mode": "Borderless"`.
- Docs in the same change: **I11** (its `ToString(EWindowMode)` exception is retired, and the parse
  direction IS unified for an enum carrying a value list — the per-enum fallback policy it cited was
  `BackendFromString`'s coercion, which survives under its own name); **I15** (the enum drawer is one
  constrained specialization, not one per enum).

## Explicitly NOT in this slice

- **Converting `Log.Level`** — needs `ToString(ELogLevel)` and a file-value case change, for a field with
  no reader. Trigger: the first thing that reads it (a logger that boots after config).
- **A bitmask/flags editor.** `EEventCategory` and `EPropertyFlags` are bitmasks; a combo is the wrong
  widget and no field of either is authored. Trigger: the first authored flags field.
- **Validating that `Values` lists every enumerator.** Nothing can check it in C++20 — a forgotten
  enumerator simply never appears in the dropdown. The list sits directly under the enum so the omission
  is visible at the only place it can be made.

---

## Review — LANDED 2026-08-20 (uncommitted)

Built as planned, no design deviations. `using enum` inside the specialization worked on MSVC first try,
so the call site is the unqualified list the plan promised.

**One thing the plan did not predict, and it is the third of its kind this arc:** making
`Render.Backend` a real `EBackend` broke the LINK. `ToString(EBackend)` and the new
`ResolveSupportedBackend` are defined in `BackendFactory.cpp` inside the DLL with no `OPAAX_API`, and
had only ever been called from engine TUs — the moment the config field held an enum, every TU that
serializes a config called `ToString`, tests included. **I6**'s tell exactly: exported-ness only ever
exercised from one side of the boundary. Both are `OPAAX_API` now, and the general rule is written into
**I11** — *making a field a real enum exports its `ToString`*.

**Gates.**
- Tests **357 / 6602**, 0 failed (up from 353 / 6593 — six new enum cases; the `WindowModeFromString`
  cases went away with the function).
- `OPAAX_BUILD_OK` on all three presets.
- **The sharp one: `git status Sandbox/Configs/` is EMPTY after booting `Sandbox.exe`.** Two fields
  stopped being strings and the file did not move a byte, which is the whole claim about writing labels
  rather than ordinals. The boot also read them: `Creating window Opaax Engine (1280, 720) [Windowed]`.
- Editor smoke: `configDrawers=2`, all four generic drawers, **0 err/warn**, and neither the configs nor
  the maps were touched by the run.

**Deleted:** `WindowModeFromString` (its fallback with it), `BackendFromString`'s parsing half, and the
I11 exception that kept `ToString(EWindowMode)` away from its enum.

**Still owed (interactive):** Window → Config, confirm `Mode` and `Backend` are combos, pick
`Borderless`, Save, and check `git diff Sandbox/Configs/Engine.config` shows exactly that one line.
