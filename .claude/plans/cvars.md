# Block CV — Console variables (CVars)

> Status: **SHELVED 2026-09-25 (the user's call: *"maybe cvar are too much for now"*).** Configs cover
> today's needs: a reader already caches `&Config.Get<T>().GetData()` and reads fields directly
> (`WorldManager.cpp`, `GameInstanceManager.cpp`), and a config can be live once readers are notified
> (block CN). **Un-shelve when either trigger fires:** (1) engine values must be set BY NAME FROM TEXT —
> command-line overrides or an in-game console in `Game.exe`; (2) developer toggles pile up that must
> NOT live in committed `.config` files (per-user state). The design below stays valid for that day.
>
> Was: PLANNED 2026-09-25, not started. No branch yet (`feature/cvars` from a fresh `main` at S0).
> Contract touched: **I1**, **I3**, **I4**, **I5**, **I6**, **BO**, **§LOG**, **F4b**. New section on landing: **§CV**.
> Reference: Gregory §5.5.3.1 (Quake CVARs), §5.5.3.3 (Uncharted in-game menu), §9.3–9.4 (menus / console).

---

## What a CVar is here — one sentence

**A named, typed, live value that a developer can change while the app runs, by name, from text.**
It is not a config field (those are project data, read at boot) and not a command (a verb).

| | Config (`IConfigSystem`) | CVar (`ICVarSystem`) |
|---|---|---|
| Answers | what the project **IS** | how this session **RUNS** |
| Lives in | `<ProjectRoot>/Configs/*.config`, **committed** | memory; optionally `SaveDir()/CVars.json`, **per-user, never committed** |
| Read | once, at boot (`NeedRestart`) | live, every frame if needed |
| Edited by | Config panel, hand edit | CVar panel, command line, (later) console line |
| Addressed by | C++ type (`Get<Config_Engine>()`) | handle in code, **name** from text |

**The boundary rule (D9):** a setting becomes a CVar only if (a) its reader takes a change live, and
(b) it is a developer knob, not a property of the game. Never both: one setting, one home. Turning on
collider outlines must not dirty a file in git.

---

## Decisions

**D1 — An app service, not a fourth I1 singleton.** `ICVarSystem` in `Application/Services/`, beside
`IConfigSystem`, whose shape it mirrors (storage in the base, the real one adds IO, `Null()` per **I3**).
Configuration is Core-Systems layer, passive (**I4**, [[L1]]).
- **Rejected: Unreal/Quake static registration** (`static TAutoConsoleVariable<int> CVarFoo(...)` at file
  scope, Quake's global linked list). It needs a global registry reachable during static init — a
  singleton I1's closed list does not have and SG2 does not justify: every reader can hold a handle,
  and every registration site already reaches the locator. It also brings back static-init order
  across the DLL/exe line and a registry per module ([[L4]], **I2**), and makes tests share state.
- Consequence: **registration is explicit, at a boot seam, and the handle is a member.** That is the
  one visible difference from Unreal, and it is the price of I1.

**D2 — The registry OWNS the value; code holds a typed handle and PULLS.**
- `TCVar<T>` is a header-only view (**I6**) over a registry-owned, address-stable `CVar` entry
  (`TDynArray<TUniquePtr<CVar>>`). A read is one pointer load — no lookup, no lock.
- **Lifetime is the reason:** the service outlives `IEngine` (**I5**), so a subsystem's or game
  module's handle cannot dangle. The reverse shape — a "ref" CVar pointing into a subsystem member
  (Unreal's `FAutoConsoleVariableRef`, Uncharted's `&g_var`) — dangles the day that subsystem restarts
  while the panel still lists it. **No ref CVars.**
- **Push only when a change must DO something** (call an API, rebuild a thing): `OnChanged()` is a
  `TMulticastDelegate`; subscribers `AddMember(this, …)` and `RemoveAll(this)` in `TearDown` ([[L7]]).
  Fires only on an actual change, and once at subscription is NOT implied (the subscriber reads
  `Get()` itself).

**D3 — Value types: `bool`, `Int32`, `float`, `OpaaxString`.** One non-template `CVar` class holding a
`std::variant` (exportable, I6); `TCVar<T>` picks the alternative. Text in/out is per type; `bool`
accepts `0/1/true/false/on/off`, case-insensitive. Bad text refuses and keeps the value.
- **Reflected enums** (`OPAAX_ENUM_VALUES`, dropdown + label text) come WITH their first consumer, not before.

**D4 — Metadata reuses `PropertyMeta`** (range, drag step, tooltip) so the panel's number widgets are
the Inspector's. A range CLAMPS on every write path (panel, text, command line). CVar-only behaviour is
its own `ECVarFlags` — not added to `EPropertyFlags`, which describes fields:
- `Persist` — written to the per-user file (D8).
- `DevOnly` — in a ship build (`OPAAX_DEV_BUILD` off, **I12**) the value is pinned to its default and
  every text write is refused. Code reading it still compiles and runs.
- `ReadOnly` — shown, never written from text (a readout).

**D5 — Names: `Group.Name`, PascalCase, dotted** (`Debug.Colliders`, `Render.Interpolation`), the
config groups' vocabulary. Lookup from text is **case-insensitive**; registering two names that differ
only by case is refused. Linear scan in registration order — dozens of entries, the same call
`IConfigSystem` made (**BO1a**). The panel groups by the prefix.

**D6 — Never seals** (BO1a's rule). Registration may happen at any time; the panel reads the list live.
Registering an existing name **with the same type returns the existing entry** (Quake's `Cvar_Get`), so
a subsystem that restarts keeps the session's value. A **type mismatch** is a programmer error: one
Error naming both types, and the second handle is bound to a detached entry holding its own default
— never null (I3's spirit), never aliasing the wrong type.

**D7 — Where a value comes from, lowest to highest:** `Default` (code) < `Saved` (per-user file) <
`CommandLine` < `Runtime` (panel / console). The entry records its `SetBy`; the panel shows it, which
answers "why is this ON?" without a debugger.
- Command line: `-cvar Name=Value` (repeatable). Parsed when the service is constructed, before any
  owner has registered, so the values are **held by name and applied at registration**.
- Unmatched overrides get **one summary line** after boot (`2 cvar override(s) matched nothing: A, B`)
  — a count, per **§LOG**, and the line that discriminates a typo from a success ([[L15]]).

**D8 — Persistence (fork, see below): session-only by default; `Persist` writes `SaveDir()/CVars.json`.**
Only `Persist` entries whose value differs from default are written, so a changed default in code
still reaches the user. Read at construction (as pending, like D7), written once at service shutdown.
Never under `Configs/` — that directory is committed project data.

**D9 — The config/CVar boundary** — the table above. **Rejected: auto-expose every reflected config
field as a CVar** (`Engine.Physics.Gravity` for free from `OPAAX_PROPERTIES`). Tempting, but those
fields are read at boot, so the live value would lie, and a panel write would either dirty a committed
file or silently diverge from it. Two writers, one setting.

**D10 — Editor: a `CVars` panel.** Registered through the panel registry like every other panel, drawn
through the editor UI seam (**MR2d**), no ImGui in the engine.
- One row per CVar, grouped by prefix: widget by type (checkbox / drag with the range / text), a
  modified marker + reset-to-default, `SetBy`, the tooltip; `ReadOnly` rows disabled; `Persist` rows marked.
- A search box: case-insensitive, over names and tooltips. The filter is header-only and tested
  (`CVarFilter.h`), the `LogFilter.h` pattern.
- **An existing toolbar toggle becomes a VIEW over its CVar**: the button writes the CVar, so there is
  one writer and the panel and the toolbar can never disagree.

**D11 — Out of scope, named:** console **commands** (verbs: `World.Pause`, `Quit`) — the editor already
has `EditorCommandRegistry` for its verbs; engine verbs arrive with an in-game console, which
`Game.exe` cannot draw today (no ImGui outside the editor). Undo for CVar edits (config undo was
deferred by the user; same answer). CVars read from worker threads: **main thread only**, stated in the
header — the resource loader never reads one.

---

## Migration — "useful settings", priced from the code

| Candidate | Today | As a CVar | Cost | Verdict |
|---|---|---|---|---|
| **Colliders** | `DebugDraw` Physics channel, toggled only by the editor toolbar; resets every launch; unreachable in a dev `Game.exe` | `Debug.Colliders` bool, `Persist`; `OnChanged` → `DebugDraw::SetChannelEnabled` (F4b's central toggle stays the renderer's truth, the CVar is its one writer); toolbar writes the CVar | ~30 lines | **Yes** — the first consumer |
| **Render interpolation** | `EngineConfigData::Render.bInterpolation`, copied once in `RendererManager::Startup` | `Render.Interpolation` bool, read per frame | ~20 lines + the key leaves `Engine.config` (format change, tolerated on read) | **Your call** — knob (A/B it while watching) or project property? |
| Time scale | does not exist (⑦ remainder) | `World.TimeScale` float, range [0, 4] | its own block | **Next block's first float consumer** |
| Grid / Snap | `EditorViewport` / `EditorGizmo` state | — | — | **No** — per-viewport tool state, and there can be two viewports (MV) |
| Pause / Step | `WorldManager` | — | — | **No** — verbs, not values (D11) |
| Window, backend, physics world, renderer limits, stats-in-ship | config, read at boot | — | — | **No** — project properties, `NeedRestart` (D9) |
| Log minimum level | the Logger has no level filter | `Log.MinLevel` enum | Logger feature + first enum CVar | **Only if you want it** |

---

## Boot order (BO amendment)

```
Platform → Paths → [Logger::Init] → Config(+PreRegisterConfig) → CVars(Paths, argv)
        → ProjectManager → JobSystem → [Profiler::Init] → WindowManager → Engine → OnProvideServices()
```
After Config so it sits with its sibling; needs only `Paths` (the per-user file) and `argv` (D7).
Shut down after the Engine (reverse order), so every owner has unsubscribed before the store goes.

---

## Files (planned)

```
Engine/Source/Core/CVar/CVar.h / .cpp              value, meta, flags, text parse/format, OnChanged
Engine/Source/Core/CVar/TCVar.h                    header-only typed handle (I6)
Engine/Source/Application/Services/ICVarSystem.h / .cpp   registry + null + real (overrides, persistence)
Editor/Source/Editor/Panels/CVarFilter.h           header-only, tested
Editor/Source/Editor/Panels/CVarPanel.h / .cpp
Engine/Tests/Core/CVar/CVarTests.cpp               ─┐ both added to Engine/Tests/CMakeLists.txt
Engine/Tests/Core/Application/ICVarSystemTests.cpp ─┘ (explicit list — check the CASE COUNT)
.claude/howto/cvars.md                             the recipe ("add a CVar", "make a toggle a view")
```

## Shape at a call site (illustrative, not final)

```cpp
// registered by the OWNER of the state it drives — here the Engine, which owns DebugDraw (not the
// per-world physics producer: one registration, one subscription). The handle is a member.
m_Colliders = CVars.Register<bool>("Debug.Colliders", false,
                  CVarDesc{}.SetTooltip("Outline every ColliderComponent.").SetFlags(ECVarFlags::Persist));
m_Colliders.OnChanged().AddMember(this, &Engine::ApplyColliders);   // RemoveAll(this) in TearDown

// hot path — one load
if (m_Colliders.Get()) { ... }
```
```
SandboxEditor.exe -cvar Debug.Colliders=1 -cvar Render.Interpolation=off
```
