# OpaaxEngine — Architecture Contract

> **What this file is.** The *positive* statement of the engine's load-bearing invariants — the rules
> I check every change against. `lessons.md` holds the *negative* record (mistakes + why). This holds
> the *contract*. Read both at session start.
>
> **Rule IDs (`I1`, `L1`, `S1`…) are citable.** In review I say "violates **I1**" instead of re-arguing it.
> If a change must break an invariant here, that is a STOP-and-re-plan trigger (CLAUDE.md §1) — we amend
> this file deliberately, we don't drift past it.
>
> **Scope:** the *new* engine (branch `refresh_engine`). The old world (`CoreEngineApp`, `*Old` classes,
> `EventOld/`) is dead-but-compiled — see **X1**. When code and this file disagree, the code wins *and you
> fix this file in the same change*.

---

## I — Prime invariants (non-negotiable)

**I1 — One static root.** `OpaaxApplication::m_Services` (a `static AppServiceLocator`) is the *only*
mutable static in the engine. Everything else is instance-owned beneath it: the locator owns the services
(`UniquePtr<IAppService>`), the `IEngine` service owns the `EngineSubsystemMgr`, the manager owns the
subsystems. No `s_Data`, no function-local `static`, no singletons past the locator. This is the invariant
the whole design optimizes (see **L2**); a proposal that adds a static is wrong by default, even a "clean" one.
*The old static `RenderCommand`/`IRenderAPI` facade was **retired to `Legacy/RHI`** (2026-07-22, [[L14]]) —
the render path is now instance-owned via `IRHIDevice` (`RenderSystem::m_Device`), zero facade statics; do not resurrect it.*

**I2 — DLL-safe type identity.** A type's tag must be a *single instance across the DLL/exe boundary*.
Two proven ways to get that — the deciding factor is **whether the tag is dll-exported**, not inline-vs-`.cpp`:
- **Out-of-line (services):** `OPAAX_SERVICE_TYPE(IFoo)` *declares* `StaticTypeID()` in the header; the
  interface's `.cpp` *defines* it. One tag, shared. `ServiceTypeID = uintptr_t`.
- **Exported-inline (subsystems):** `OPAAX_SUBSYSTEM_TYPE` keeps `StaticTypeID()` inline (a function-local
  static). It is still single-instance *because the subsystem class is `OPAAX_API`* — dllimport imports the
  one exported inline definition into the exe rather than re-emitting it. **Proven by S9 (2026-07-20):**
  `GetSubsystem<WorldManager>()` is non-null across the exe/DLL line (DLL-side instance tag == exe-side
  `StaticTypeID()`, same address). So I2 is *not* "out-of-line only" — it is "one exported tag."
- The "duplicates per module" hazard (**L4**) bites a tag that **two modules both instantiate** — a
  header-only template static, or a type compiled into the DLL *and* the exe. The axis is **how many
  modules emit the tag**, not whether it is exported. If `GetSubsystem<T>()` across the boundary ever
  returns null, this is still the first suspect.
- **A NON-exported subsystem is fine when it lives in exactly one module — proven by the M4 S1 probe**
  (2026-07-29, `Engine/Tests/Core/World/WorldSubsystemIdentityTests.cpp`). This corrects an earlier,
  broader caveat here which claimed a subsystem in a **static lib / game module** "would get a per-module
  copy" and must go out-of-line or be exported. It does not: a game type is compiled into the **exe
  only**, so there is no second copy to disagree with, and the exe linker folds the per-TU COMDATs of the
  inline `StaticTypeID()` into one address. The probe pins it the way M4 needs — probes defined in a
  header used from **two** TUs (a single TU cannot tell "one tag per type" from "one tag per TU"), with
  external linkage (an anonymous namespace would have guaranteed the wrong answer, [[L21]]), registered
  in one TU and resolved in the other through a DLL-owned `WorldSubsystemMgr`. Structurally identical
  probe types get distinct tags, and an exe-side tag does not collide with `WorldManager`'s DLL-exported
  one. **So a game module needs no ceremony: derive, stamp `OPAAX_SUBSYSTEM_TYPE`, done.** Until this
  probe, every live user of that macro was an `OPAAX_API` engine subsystem, so the non-exported case had
  never once been exercised — the "S9 proof" above covers only the exported case ([[L21]]/[[L22]]: the
  contract's own claim was a premise, not a finding).
- **Exported-inline is for *tags*, NOT for shared mutable STATE.** S9 proved MSVC imports an exported
  inline function's local static for an identity tag — where the only requirement is address identity, and a
  wrong answer fails *loudly* (a null `GetSubsystem<T>()`). A function-local static holding a **mutable
  table** is a different risk class: if a module emits its own copy nothing crashes, the two tables simply
  disagree, silently and forever. State-bearing accessors therefore go **out-of-line in the DLL**, where one
  definition is a *link-time guarantee* rather than a compiler courtesy. Applied 2026-07-27 to
  `OpaaxStringID::GetPool()` (the intern table): pool + its entry points (interning ctor, `ToString`) moved
  into `OpaaxStringID.cpp`, and `OpaaxStringIDPool` reduced to a forward declaration so a consumer cannot
  even see its layout. Pool-free members (comparison, `GetId`, `IsValid`) stay inline. Had this drifted, the
  *same string* would intern to different `Uint32`s across the DLL line — and since every `OpaaxStringID`
  compare is an integer compare, it would have failed **silently**. Guarded by `Core/StringIDTests.cpp`.
- New cross-module identity must hash a compiler-stable per-type string (`__FUNCSIG__`), never a
  template-static counter.

**I3 — Resolution never returns null.** `AppServiceLocator::Get<T>()` returns `T::Null()` (an inert null
object), never `nullptr`. Every service interface provides `static T& Null()` + `IsNull()`. Callers may use
a service unconditionally; they check `IsNull()` only when absence is a *real* branch. Do not add
null-pointer checks around `Get<T>()`.

**I4 — Gregory layer split decides App vs Engine.** Before placing any system, name its layer in Gregory's
runtime diagram (Fig 1.16):
- **App service** = Platform-Independence + Core-Systems layers — *passive facilities* you submit-to/query:
  Platform, Paths, Logger, Config, ProjectManager, JobSystem, WindowManager. They do not tick.
- **Engine** = Resources/Assets layer *and up* — anything that ticks per frame or owns game concepts
  (Resources, Renderer, World, Physics, Input). Lives as an `EngineSubsystem`, never as an app service.
- Test: *ticks per frame or knows about textures/worlds* ⇒ engine. *Passive facility* ⇒ app service.
  (See **L1**.) `IEngine` is itself an app service — a deliberate exception that serves **I1**, not a
  counter-example to this rule.

**I5 — Ownership follows lifetime, top-down.** Four tiers, each owned by the one above, destroyed
bottom-up: `static locator → app services → IEngine → engine subsystems`. A subsystem never owns a
sibling; it *borrows* one by resolving from the manager (**F3**). Convenience pointers to siblings are
non-owning, initialised `= nullptr`, and re-resolved, never `delete`d.

**I6 — Never dll-export a class *template*; stateless value types are header-only.**
`__declspec(dllexport/dllimport)` (`OPAAX_API`) on a class *template* exports nothing — a template is not
code until instantiated, so marking it `dllimport` makes every consumer expect the instantiation *from the
DLL*, which never exports it → **LNK2019** (proven 2026-07-20: the `Angle` module linked from the editor exe
against the engine DLL). Two correct shapes:
- **Header-only value template** — `TFloatValue`, `TDegree`/`TRadian`/`TAngle`, math vectors: **no
  `OPAAX_API`.** Instantiated per-TU; DLL-safe *by construction* — no shared state, no vtable, no identity
  tag to unify (the opposite end of the axis from **I2**: identity/export matter only for types with shared
  state or a cross-module tag; a stateless value has neither). Out-of-line member defs go in a `.inl` that
  is **`#include`d where the type is declared**, or they're unresolved too.
- **Explicit instantiation** (only to hide a body / cut bloat): `template class OPAAX_API TFoo<float>;` in a
  DLL `.cpp` + `extern template class OPAAX_API TFoo<float>;` in the header. Overkill for a solo 2D engine's
  small value types (project value **Simple**) — prefer header-only.
`OPAAX_API` belongs on **non-template** classes with real compiled members (services, the `Maths` static
struct, `Engine`), never on the template itself.
- **Second proof, and a latent violation fixed (M4 S1, 2026-07-29): `ISubsystemManager<T>` was exported.**
  Its *member templates* (`RegisterSubsystem<T>`, `GetSubsystem<T>`) linked fine — MSVC always instantiates
  those locally — but its **non-template** members (`StartupAll`, `ShutdownAll`, …) were `dllimport`, so a
  consumer expected them from the DLL, which never exported that instantiation → **LNK2019**. This hid for
  as long as only `Engine.cpp` (*inside* the DLL) drove a manager; the first outside caller surfaced it.
  Fix per this invariant: drop `OPAAX_API` from the template (every member is inline in the header, no
  static, no identity tag — the header-only shape) and keep it on the **derived** `EngineSubsystemMgr` /
  `WorldSubsystemMgr`, which are non-template classes. No C4275, no explicit instantiation needed.
- **The tell for this class of bug: a template whose exported-ness is only tested from inside the DLL.**
  An `OPAAX_API` template compiles and links indefinitely while every caller is DLL-internal, because
  dllexport-side instantiation is what the DLL does anyway. It fails the day something outside calls a
  non-template member. When exporting reaches a template, ask *who will call this from the exe* — the
  answer "nobody yet" is how the defect stays latent.
- **Corollary (M3): `OPAAX_API` instantiates every IMPLICITLY-declared member**, so an exported class
  holding a move-only member (`TDynArray<UniquePtr<T>>`) fails to compile on its implicit *copy*-assign
  (C2280) even though nothing ever copies one. Declaring copy/move `= delete` is therefore **required**,
  not hygiene — the shape `World` already uses, now also `ComponentRegistry`.

**I8 — A component is defined by a CONCEPT, and there is NO component base class** (landed M3,
2026-07-28). `CComponent` (`World/Components/ComponentConcept.hpp`) requires nlohmann
`to_json`/`from_json` by ADL — the same concept-over-base-class shape as `CResource`, and the only shape
available: **entt stores components by value**, so `Save`/`Load` cannot be member virtuals the way the
retired `Legacy/ECS/ComponentRegistry` did it. A game component costs one
`NLOHMANN_DEFINE_TYPE_INTRUSIVE` and one `Components().Register<T>()`; the engine names it nowhere.
The empty `ComponentBase`/`IComponent` markers were **deleted** once the concept took over their stated
job (user call): an empty non-virtual base is an attractive nuisance — the first person to add a virtual
to it silently breaks by-value storage and nothing complains. **Do not reintroduce one.** Per-entity
behavior is authoring data + a world subsystem (D7), never a polymorphic component.
`EntityMeta` deliberately does **not** satisfy it — identity is not user data, and the snapshot core writes
those fields by hand rather than nesting an entity's identity inside its own payload.

**I7 — Every string the engine carries is UTF-8; conversion happens at the PLATFORM boundary** (landed
2026-07-28). `OpaaxString` is a byte container with no encoding of its own, so the encoding is a
*convention* — and the platform layer already fixed it by converting `GetExecutablePath` through
`CP_UTF8`. Therefore any code that hands an engine string to an OS or CRT entry point must convert
explicitly, because the narrow entry points do **not** assume UTF-8:
- On MSVC, `std::filesystem::path(const char*)` and every `*A` Win32 function decode using the **ANSI
  code page**. Feeding them UTF-8 resolves a *different file than the caller named*, with no error
  anywhere — `IsPathExist` answering false for a directory that exists (proven, `WindowsFileSystem`).
- **The conversion is `Core/String/OpaaxUtf8.h`** (`Opaax::Utf8::ToFsPath` / `FromFsPath`, plus
  Windows-only `ToWide`/`FromWide`). It lives in **Core**, not in the platform folder, because the layers
  that need it — Core's config IO, the portable `Renderer`'s shader loading — sit *below* Application and
  cannot reach `IPlatform`. **Never build an `fs::path` or open an `fstream` from `OpaaxString::CStr()`;**
  `std::fstream` takes an `fs::path` (C++17), so obeying this is a one-line call. Interfaces above the
  platform state UTF-8 in their contract and never see a `wchar_t`.
- **`IFileSystem` is one CONSUMER of this rule, not the only legal way to touch a file.** It is reachable
  only through the service locator, which Core and Renderer must not use; forcing them through it would
  mean injecting a filesystem into `TConfig` and the `CResource` contract. The encoding is the invariant;
  the facility is not.
- **This fails silently and symmetrically, which is why it needs an invariant rather than care.** A
  mis-encoded write followed by a mis-encoded read agrees with itself; only checking against the OS's
  *wide* API reveals the truth. Tests that pin encoding must use `\uXXXX` escapes, not literal
  characters — the build sets no `/utf-8` and the sources carry no BOM, so a literal would be decoded
  by the very mechanism under test.

**I9 — A constant lives with the CONTRACT that owns it; a shared file is only for constants NOBODY
owns** (settled 2026-08-05, rejecting a proposed central `OpaaxStatics.h`). Two shapes, both already in
the tree and both correct:
- **Owned by one contract → its header.** `MAX_FRAME_DELTA` in `Engine.h` (**F1**'s clamp),
  `NULL_LEVEL_WORLD_NAME` beside `WorldSpec` (**BO4c**), `ENTITY_NONE` in `EntityTypes.h`,
  `LEVEL_EXTENSION`/`KEY_*` in `LevelFile.h`. Anyone who needs the constant already includes the header.
- **Shared domain, owned by nobody → a statics file.** `Core/Maths/MathsStatics.h`. Everyone needs PI,
  no module owns it, and it never changes — so the "one header rebuilds the tree" cost is nil.

`Engine::Loop`'s fixed step shows the split working on one line: the timestep is `D60_HZ` from
`MathsStatics` (a generic *ratio*), the clamp is `MAX_FRAME_DELTA` from `Engine.h` (a *policy*).
**Graduation trigger:** two unrelated modules need it and neither owns it — until then, one home.
Centralizing instead would **invert layering** (Core would hold a world-naming policy — **I4**) and
**group by storage kind rather than meaning** (the [[L30]] shape: `NULL_LEVEL_WORLD_NAME` next to
`MAX_FRAME_DELTA` tells a reader nothing). That decentralized form already scales here: `LogCategory` is
a constant, there are ~10 of them declared per-header via `OPAAX_LOG_CATEGORY`, and nobody has ever
wanted them in one file.
- **`OpaaxGlobal` is a NAMESPACE of constants, not a struct of statics** (2026-08-05). It held two
  out-of-line `static const` members, so `ID_None`'s *value* was invisible to consumers and
  `OpaaxStringID`'s default ctor could not be `constexpr` while its `operator==` was. `ID_None` is now
  `inline constexpr` (header-only, no export, no `.cpp`) and the ctors / `GetId` / `IsValid` are
  `constexpr`. `String_None` stays out-of-line — an `OpaaxString` is not a constant expression, and it
  is read only inside the DLL.

**I10 — An engine type keeps its `Opaax` prefix; do not alias it away** (settled 2026-08-05, deleting
`Core/OpaaxForward.hpp`). Note which direction the aliases in `OpaaxTypes.h` run: `TDynArray`,
`UniquePtr`, `Mutex` take a **std** type and *add* engine identity. `using String = OpaaxString` ran the
other way — it *stripped* identity from a type that already had it, so a reader seeing `String` had to
know the alias existed, which is the exact clarity the prefix buys. `OpaaxTypes.h`'s own header comment
already named `OpaaxString` canonical, and `OpaaxHash.h` has a parameter literally named `String` inside
`namespace Opaax` — a shadow that stayed harmless only because one file included the alias.
- That file was also **not a forward header**: it `#include`d the full definition, so it forward-declared
  nothing and broke no cycle, despite saying so. **A real forward header is `class Foo;` with NO
  includes.** The tree's ~175 *local* forward-decl lines (e.g. `WorldManager.h`'s four siblings) are the
  better pattern — precise, self-documenting, nothing to keep in sync. Reach for a per-module `…Fwd.h`
  only when a specific header's include cost shows up in build times, never preemptively.

---

## LC — Lifecycle: three states, not two

The engine has **three** states, and the middle one is the doctrine that keeps cleanup correct (**L7**):

| State | When | Guarantee | What may run here |
|-------|------|-----------|-------------------|
| **Running** | the frame loop | everything live | Update / FixedUpdate / Render / Present |
| **TearDown** | loop stopped, nothing destroyed yet | window, GPU, bus, **all siblings still alive** | release anything that needs a *live sibling*; broadcast "X-ending" events |
| **Shutdown** | reverse-order destruction via locator | siblings may **already be gone** | free only your *own* resources; never reach out |

**LC1 — The missing-phase rule.** If a cleanup problem has no good answer inside the phases you have, the
fix is usually a *new phase*, not defensive code in a destructor. The real axis is **"all alive" vs
"things dying"** — no reordering *inside* Shutdown can fake TearDown. This is one doctrine at every scope:
`ISubsystem::TearDown` / `IEngine::TearDown` / `OpaaxApplication::EngineTeardown`. Two-phase events wear
the same hat: broadcast `XCreated` *after* init, `XDestroying` *before* deinit.

**LC2 — Teardown-time events use `Publish`, not `Enqueue`.** The queue is flushed inside `Loop()`, which
has already stopped by TearDown; an *enqueued* teardown event is never delivered. Bridge it with `Publish`.

**LC3 — Idempotent + reverse order.** Startup is registration order; Shutdown/TearDown are reverse.
`Shutdown` is idempotent. Destruction is **reverse order of PROVISION** — and the last service provided
is *not* always `IEngine`. The base `Bootstrap` ends with it, but the `OnProvideServices` seam runs
**after** that, so a host service registered there tears down **before** the engine: the runtime order
is `IEngine → WindowManager` (no host overrides the seam), the editor's is
`IEditorService → IEngine → WindowManager`.
*Corrected 2026-08-05: this rule read "`IEngine` is provided last, so it tears down first", which is
false in the editor and misdescribes the mechanism the editor relies on.* That gap is where
`ViewportPanel::Shutdown` frees a panel-owned FBO while the engine, its device and the GL context are
all still alive — the device is freed later still, in `RendererManager::Shutdown`. Verified in the
editor's shutdown log: `Engine torn down` → `ViewportPanel shutdown` → `RendererManager shutdown`.
**`RendererManager` deliberately has NO `TearDown`**, because the LC table above promises every
subsystem that the GPU is alive for the whole of TearDown; releasing the device there would break that
promise for each of them. The two phases exist so the device outlives everything holding GPU resources.

---

## BO — Boot order (app services, in `Bootstrap()`)

Provided into the locator in strict dependency order — do not reorder without cause:

```
Platform → Paths → Logger(Paths) → Config(Paths)+PreRegisterConfig
        → ProjectManager(Paths) → JobSystem → WindowManager → Engine → OnProvideServices()
```

**BO1** — Config is loaded from disk here, *before* the Engine exists. JobSystem comes *after* Config
(worker count is config-driven). (See **L1**.)
**BO2** — The Engine service is **constructed** in `Bootstrap` (pre-window) but **started**
(`Engine().Startup()`) later, in `EngineStartup()`, *after* the window exists.
*(This supersedes L1's note that IEngine is created post-window — it is created in Bootstrap, started post-window.)*
**BO3** — The **window** is created in `InitializeApplication()`, not Bootstrap — it needs a live GL/VK context.
`WindowManager` (the service) is booted in Bootstrap; the window object comes later.

**BO4 — Starting a subsystem is INFRASTRUCTURE; creating a world is CONTENT, and the host does it last**
(landed M3, 2026-07-28 — user correction). `EngineStartup` reads as three plain stages:

```
Engine().Startup()                     → every subsystem constructed and started. NO WORLD EXISTS.
RegisterModules(...)                   → components -> ComponentRegistry, world subsystems -> its registry
OnModulesRegistered()                  → editor extensions; editor seals its own registries
Engine().FinishStartup(GetStartupWorldSpec())
                                       → host says WHICH; engine creates it. The first CreateWorld
                                         SEALS the registries, then makes the world.
```

**`Startup` and `FinishStartup` BRACKET the registration window** (M4 S2, 2026-07-29). Between them every
subsystem is up and no world exists — the only moment a module may register a type. That used to be stated
only in a comment; now the API says it, which is the whole point of the second name.

`WorldManager::Startup` **must not create a world.** It used to spin up a default "Main" so there was
"always a render target" — dead weight, because every consumer already handles no-active-world
(`RendererManager::Render` guards; `HierarchyPanel` renders "No active world."). What it *did* do was make
the boot order unfixable: the first `CreateWorld` seals `ComponentRegistry`, so a world born inside
subsystem startup locked the registry before any game module could reach it.

**The host states POLICY, the engine performs MECHANISM** (M4 S2 — two user TODOs in the tree). The seam is
a **pure query**: `virtual WorldSpec OpaaxApplication::GetStartupWorldSpec() const` returns
`{ Name, EWorldMode }` and *does nothing*; `IEngine::FinishStartup(spec)` does the `CreateWorld` +
`SetActiveWorld`. The old `CreateStartupWorld()` seam is **deleted** — it reached through the engine to drive
a subsystem (`Engine().GetWorldManager().CreateWorld(...)`), the same smell **MR0** removed for registries.
A side-effect-free query is also testable without booting an engine, and a host can no longer half-create a
world. `FinishStartup` **refuses loudly** (Error + null) if called before `Startup` — no lazy safety net,
because that is precisely what silently reordered the boot in [[L22]].

**The world comes from config, not from code**: the base `GetStartupWorldSpec` reads
`IProjectManager::StartupLevel()` — the project's `.opaaxproj`, key `startupLevel`, with `startupScene` /
`defaultScene` as Scene-era fallbacks (**X4**). Hosts override to open something else.

**BO4b — `WorldSpec` carries a `LevelPath` and a `Mode`, and NO `Name`: a world is named by the LEVEL it
opens** (M5 2026-08-03; naming reworked 2026-08-04). The seam briefly carried both, with `Name` **derived
from the path's stem** — and mining a name out of a file path is the wrong source, because it makes the
world's identity a property of where the file happens to sit rather than of what the author called it.
`LevelData::Name` is now that source: `LevelFile::Load` fills it from the `name` key, falling back to the
file's stem so an unnamed level still names its world. A host that cannot supply a level therefore cannot
invent a name for one either, which is why the field left the seam entirely along with
`OpaaxApplication::DeriveWorldName`.

**The consequence is an ORDERING one: `FinishStartup` reads the level BEFORE it creates the world**
(`Engine::ResolveStartupLevel` → `CreateWorld` → `OpenStartupLevel`). Only the *read* moved forward;
**instantiation still happens after `CreateWorld`, so WS7 is unaffected** — the boot log shows the world's
subsystems starting before the entities land, the same order a PIE clone gets.

**BO4c — A startup level that does not resolve is a FALLBACK, never a refusal to boot.** Absent, empty or
unreadable, the engine creates a world named **`NullLevel`** (`NULL_LEVEL_WORLD_NAME`, beside `WorldSpec`)
and opens nothing. The two cases are deliberately different volumes: an **empty `LevelPath` logs Info**
(a test host, or a game that fills its world in code — a supported answer, unchanged), while a path the
project **names but cannot open logs Warn** (a real misconfiguration). Resolution goes through
`ResourceManager::Load<LevelResource>`, whose FailFast null **is** the existence check — and it covers a
corrupt file too, which a bare path-exists test would wave through.
*The general lesson is [[L30]]'s: a field two things were sharing only because one of them was always
empty is a latent bug, not a simplification.*

**BO4a — `EWorldMode` is fixed at construction, and that is load-bearing** (M4 S2). A `World` takes its mode
in the ctor and exposes `GetMode()` with **no setter**; changing mode means creating another world. That is
what makes PIE-by-clone coherent: Play runs a *clone*, so Stop restores the edit world by discarding the
clone rather than undoing anything. A settable mode would quietly re-introduce the "put the world back after
playing" problem cloning exists to avoid. Runtime hosts answer `Play`; `EditorApplication` overrides the
query to `Edit` and lets the base keep owning where the *name* comes from. `EWorldMode`/`WorldSpec` live in
`Application/WorldSpec.h` — **Application, not World**: no Application header may include from `World/` or
`Engine/` (**MR1**) and a by-value return needs the complete type, but the better reason is that Edit-vs-Play
is a *host* mode. The World merely records the label it was born with.

**The rejected fix is worth remembering.** The first attempt kept the world in `WorldManager::Startup` and
split `StartupAll` into `CreateAll` + start, adding an `IEngine::BootSubsystems()` phase to squeeze
registration into the gap. It worked and it was wrong: it added machinery to preserve a layering mistake
instead of removing it. Deleting the world from subsystem startup deleted the phase too. **When ordering has
no legal window, check whether something is happening in the wrong phase before inventing a new one**
([[L22]]).

**Full frame of the run:**
`Bootstrap() → InitializeApplication() [window] → RunApplication{ EngineStartup → loop → EngineTeardown } → ShutdownApplication()`

---

## F — Frame contract

**F1 — Tick order** (`IEngine`, driven by the host): `Update(dt)` → `FixedUpdate(fixedDt)` *[may run 0..N
times]* → `Render(alpha)`. Then the **host** calls `PresentBackbuffer()` — outside the engine. Delta is clamped
to `MAX_FRAME_DELTA = 0.25`.

**F2 — Present is split from Render (render north star, S7/D2).** `Render` draws; present swaps. The
**host owns WHEN** (`RunApplication` calls `Engine().PresentBackbuffer()` after `TickFrame()`); the **device
owns HOW**. Only the backbuffer is ever presented; offscreen render targets never are — the facade method is
named `PresentBackbuffer()` (not `Present`) so the call site states that invariant (M1, 2026-07-26). The
`RenderSystem`/`RendererManager`/`IRHIDevice` layer keeps the plain name `Present()` (one unambiguous
swapchain there). This gap is what lets the editor draw world→FBO, UI→backbuffer, then present once. Present
is instance-based (no statics — **I1**).
- **Render-pass shape (landed M1, 2026-07-26):** `BeginFrame / BeginPass(target,view) / EndPass / EndFrame /
  PresentBackbuffer`. `BeginFrame`/`EndFrame` bracket the device frame; the pass bracket
  (`BeginRenderPass`/`EndRenderPass`) lives in `BeginPass`/`EndPass`, so a frame renders into any
  `IRenderTarget` (backbuffer or offscreen FBO — `RendererManager` picks via `m_PrimaryTarget`, size read
  from the target). "scene" is retired vocabulary — a render *pass into a target with a view*. New render
  path is **OpenGL-only** today; the VK backend is **parked in `Legacy/RHI/Vulkan`** (2026-07-22) pending a
  new-path `VulkanRHIDevice`.

**F2a — Every GPU resource is created BY THE DEVICE** (landed 2026-07-28). `IRHIDevice::CreateXxx` —
buffers, textures, shaders, pipelines, bind groups, **and framebuffers**. The device already knows its
own backend, so nothing else has to dispatch on one; a free `X::Create` puts a `MakeUnique<OpenGL…>`
inside a backend-*neutral* TU, which is the shape a second backend has to unpick. The one surviving
free factory is **`IGraphicsContext::Create`**, and only because the context must exist *before* the
device that `Init`s against it — there is no device to ask yet. That exception does not generalize:
if a device could have created it, the device creates it.
- The editor reaches this through **`IEngine::CreateFramebuffer`**, one intent-named method — *not*
  a `GetRenderDevice()` accessor. Exposing the device to reach one factory would also hand out
  `BeginFrame`/`Present`/`CreatePipeline`; the device stays engine-internal (this is M1 fork 1's
  objection honored, not reversed — the punt it justified is what ended).
- Resources are **caller-owned** (I5) and must be released while the device and its GPU context are
  still alive — for a panel-owned FBO that means `Shutdown`, never a destructor racing LC teardown.

**F3 — A subsystem needing a sibling mid-boot resolves from the manager.** The guarantee that makes this
work is the manager's **create pass**: `StartupAll` constructs every subsystem before running any
`Startup`, so during its own `Startup` a subsystem may reach a sibling that exists but has not started.
**Never** via a lazy accessor that can re-enter the owner's boot — that is the per-frame re-init loop of
**L6**.

*Corrected M4 S3, 2026-07-29: the "`m_Subsystems.GetSubsystem<T>()`" phrasing described what **`Engine`**
does — a subsystem has no manager pointer.* A subsystem reaches a sibling through
`OpaaxApplication::GetAppService<IEngine>()` and the engine's accessors (`GetResources()`,
`GetEngineEventBus()`, `GetDebugDraw()`), which are **safe mid-boot precisely because they
resolve-from-manager first** and only fall back to a lazy `Startup()` when nothing is there at all.
`RendererManager::Startup` and `WorldManager::Startup` both do this. Resolve **once, in `Startup`**, and
cache non-owning pointers (**I5**) — not per use, and not per world.

**F3a — One scope down: a WORLD's subsystems get the same create-then-start guarantee.**
`WorldManager::CreateSubsystemsFor` creates every qualifying candidate and *then* calls one `StartupAll`,
so a world subsystem may reach a sibling world subsystem during its own `Startup`. Do not start
candidates as you create them.

**F4 — DebugDraw is immediate-mode BY CONTRACT** (landed M2c, 2026-07-27). The queue
(`Renderer/DebugDraw.h`, owned **by value** by `RendererManager` — the thing that drains it, I5) is drained
and cleared **every frame, unconditionally** — including on frames that fail to render, which is why
`Render()` clears *outside* `RenderFrame()`'s early-outs. A producer that wants a line visible re-submits it
every frame; nothing is retained. Lines render as thin rotated quads through the existing
`Renderer2D::DrawQuad` on the `ERenderLayer::Debug` band — **zero new RHI/shader/vertex-layout surface**;
keep it that way. Engine-owned, not editor-owned (D10: it serves dev builds of `Game.exe`, which never
links `OpaaxEditorLib`); reached via `IEngine::GetDebugDraw()`.

This is not an implementation detail — it is the invariant that makes a whole bug class impossible.
Unreal's `FlushPersistentDebugLines(World)` is destructive-to-everyone *because* a shared retained pool
exists to destroy: one system clearing its debug geometry wipes every other system's. With no retained
pool, "flush my draws" is "stop calling `Draw`", which cannot touch a sibling. **Selective flush is free
here precisely because there is nothing to flush.**

Three forward constraints, decided before a second producer existed:
- **F4a — If retention is ever added it is DURATION-LIMITED. Never infinite persistence.** A timed entry
  expires on its own, so state stays bounded and "flush that specific thing" stays rare rather than routine.
  Infinite-persist *plus* a global-only flush **is** Unreal's bug; do not rebuild the first half.
- **F4b — Any retained store is keyed by a channel tag from day one** (`using DebugChannel = OpaaxStringID`
  — reuses an already-`OPAAX_API`, already-interned type; adds no static, I1-clean). Then "flush a channel"
  is an erase on one bucket, never a sweep. Retrofitting selectivity onto a flat retained array is how the
  Unreal shape gets rebuilt by accident.
- **F4c — No per-draw handles.** Every case that reaches for one (a marker on the selected waypoint, the
  last N impacts) is already covered by re-submission or a lifetime, and handles cost real bookkeeping in
  every producer. Revisit only for a concrete *same-frame* retraction need.

A **channel tag for central toggling** ("hide all physics debug" from one editor checkbox) is the one thing
"stop calling" cannot give you, since the toggle must live outside the producer. Deliberately **not built**
while there is one producer (`m2-panels.md` §F3 — never an API with no caller). **Trigger:** the second real
producer (physics/collision debug), which also earns the editor toggle panel. The cross-module identity
question this raised is already **closed** — `OpaaxStringID`'s intern pool was moved out-of-line into the
DLL the same day (see **I2**), so channel ids agree across the DLL line by construction.

---

## SE — Extension seams (composition-root-only)

Only a **composition root** (an `OpaaxApplication` subclass — `Sandbox`, `EditorApplication`) overrides
these. A *service* or a *panel* never touches the locator (D3). Base implementations are silent no-ops, so
not overriding them leaves runtime byte-identical.

| Seam | Fires | Purpose |
|------|-------|---------|
| `CreatePaths()` | in `BootPaths` (Bootstrap) | choose the `IPaths` impl (runtime `Paths` vs `EditorPaths`) |
| `PreRegisterConfig()` | in Bootstrap | register config types before load |
| `OnProvideServices(locator)` | end of Bootstrap | add host-owned app services (editor adds `IEditorService`) |
| `PreEngineStartup()` | start of `EngineStartup` | before subsystems start |
| `RegisterModules(registrar)` | in `EngineStartup`, **after `Engine().Startup()`** — subsystems up, registries live, **no world yet** (BO4) | route the game module — drives `IRuntimeModule::OnRegister` (**MR**) |
| `OnModulesRegistered()` | in `EngineStartup`, **after** `OnRegisterModules`, **before** `Engine().Startup()` (still no world) | editor registers its D10 extensions and **seals before the first world** (§2). `EditorApplication` overrides → `EditorService::RegisterExtensions`, which registers the editor's own **native** panels first, then drives each `IEditorModule::OnRegister(EditorExtensionRegistrar&)`, then seals — **MR2's order one level down** (natives → game module → seal), so a native panel travels the same route as a game panel with no privileged path. Generic engine-side name (no editor types) — the engine stays editor-ignorant (**D4**). |
| `GetStartupWorldSpec() const` | in `EngineStartup`, **after `OnModulesRegistered`** — the last step of boot (BO4) | **a pure query, not an action**: answer *which level* and *which* `EWorldMode` the app starts in — **not** what the world is called (BO4b). Base impl is real, not a no-op — `IProjectManager::StartupLevel()` + `Play`; `EditorApplication` overrides → `Edit`. The engine then does the work in `IEngine::FinishStartup`, whose `CreateWorld` is the **first** one and so seals the registries. Replaced `CreateStartupWorld()`, which reached through the engine to drive `WorldManager` itself |
| `PostEngineStartup()` | end of `EngineStartup` | after subsystems start **and the startup world exists** (editor inits `EditorService`; Sandbox populates the world) |
| `TickFrame()` | per loop iter | base = `Engine().Loop()`; editor wraps it UI-begin → Loop → UI-end |
| `OnEvent(event)` | window callback, per event | base = app sink (close/resize→bus); `EditorApplication` overrides → `EditorService::RouteInput` first (S11), so the editor sees events before the bus |

---

## MR — Module registration (D9)

A game module is an **`IRuntimeModule`** (`Application/IRuntimeModule.h`); its `OnRegister` registers
**into** a `ModuleRegistrar`, invoked by the host's `RegisterModules` seam before any world exists:
```cpp
InRegistrar.Components().Register<TransformComponent>();      // → ComponentRegistry v2 (M3)
InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>(); // → WorldSubsystemRegistry (LIVE, M4 — see WS)
```
**MR0 — Registries live on `Engine`, in one `EngineRegistries` aggregate** (`Engine/Registries/`, user call
2026-07-28). Editor.md §2 has said "Engine builds the registry" since v3; M3 briefly hung `ComponentRegistry`
off `WorldManager` and that was wrong for three reasons: a registry is **type metadata**, not one
subsystem's state (the Inspector reads it too, and it is not "the world manager"); **there is more than
one** — `WorldSubsystemRegistry` lands in M4, and hanging each off whichever subsystem happens to read it
turns that subsystem into a bag; and **registration is a boot-order concern** (MR2), which is the Engine's
business. Consequences: the engine registers its **own** native types in `Engine::RegisterNativeTypes()`
before any subsystem exists; `WorldManager` **borrows** a non-owning `EngineRegistries*` (injected through
its subsystem factory, so it never reaches for the Engine) and seals it on the way to the first world; and
`BindEngineRegistries(EngineRegistries&)` is one call that does not grow an argument per registry.

**MR1** — The call-site API is **final now**; only the route *bodies* change (M0 counts; M3/M4 forward to
real registries). Do not change how modules call in. **`Components()` went real in M3** and the call site
did survive verbatim, because the authoring name is *optional*: omitted, `ComponentRoute` derives the C++
type's leaf name (`Opaax::DummyComponent` → `"DummyComponent"`). That derived name is the key written into
map files, so renaming the C++ type orphans components already saved — pass an explicit name to pin it.
Not silent when it happens: `MapFactory` warns per unknown component as it skips them.
**`ModuleRegistrar` moved to the ENGINE layer in M3** (`Engine/Modules/`): it exists to front the engine
registries, and by **I4** a registrar that knows about component types knows about worlds. It could live in
Application only while it knew nothing but a count. `OpaaxApplication` holds it behind a forward declaration
so **no Application header includes from `World/` or `Engine/`** — a discipline the tree has never broken.
**MR1a — …except where the M0 skeleton guessed a payload that did not exist yet** (M2d, 2026-07-27).
`AssetTypes().Register<TAsset, TActions>()` presumed an asset *type* to bind; the engine has no such type
(the `CResource` system is load-by-path, and `Legacy/Assets` is unlinked), so the route graduated to
`ResourceTypes().Register(ResourceTypeDesc{...})` — keyed by file extension, no template. **The test:** a
skeleton call site is binding when its payload already exists in some form (`Drawers<TComponent,TDrawer>`
— both real); it is a *guess* when it names a type nothing defines. Amend the contract rather than bend
the design to a placeholder's shape, and amend `Docs/Architectures/Editor.md` in the same change.
**MR2** — Order is engine natives → game module → editor module → **seal** (before the first world). The
editor module slots in before the seal.
**MR2a — every D10 route is REAL as of M5, and `EditorRoute` is DELETED.** The M0 counts-only skeleton
existed to make boot *ordering* observable before any machinery did; `Panels()` (M2a), `Drawers()` (M2b),
`ResourceTypes()` (M2d), `EditWorldSystems()` (M4 S5) and finally `Menus()` (M5 S4) each graduated off it,
and the type left with its last user. `MenuRegistry` stores a **flat array of `{Path, FMenuCommand}` in
registration order** and the nesting is computed at *draw* time by splitting each path on `/` — a tree
built at registration would be a second structure to keep consistent with the paths it came from. The
editor's whole menu bar is registry-driven, its own `File/*` entries registered through the very route a
game's `Tools/*` uses (D10), which also dissolves the merge problem: there is only one `File` menu because
it is assembled from paths. **The M0 placeholder could not survive this** — a command needs
`EditorContext&` to do anything (D3) and `[] {}` does not convert — which is exactly why registry,
consumer and dogfood were one atomic step ([[L16]] predicted this in M2 and it held).
**Note on the M2a-style diff gate** ("a game extension costs zero `OpaaxEditorLib` changes"): it does not
apply to the slice where the route itself goes real, and cannot. It applies again from the next entry.
**MR3 — One module shape.** Runtime and editor modules share a marker base **`IModule`**
(`Application/IModule.h`): `IRuntimeModule : IModule` (`OnRegister(ModuleRegistrar&)`) and
`IEditorModule : IModule` (`OnRegister(EditorExtensionRegistrar&)`). `OnRegister` stays on each derived
interface — the two register into *different* registrars, so it can't sit on the base. All three are
header-only pure interfaces, **no `OPAAX_API`** (no exported symbols / shared state / identity tag — the
opposite end of the axis from **I2**). A host invokes a module as a throwaway instance
(`SandboxModule().OnRegister(reg)`), symmetric across runtime and editor.

**MR4 — The EDITOR registers world subsystems through the game-side route, into the SAME registry**
(M4 S5). `EditorExtensionRegistrar::EditWorldSystems()` is an `Opaax::WorldSubsystemRoute` — the very type
`ModuleRegistrar::WorldSubsystems()` uses, not an editor parallel — bound by `EditorService` at
`OnModulesRegistered`, which **MR2**'s order puts after the engine has started and before the seal. One
candidate list ends up holding a game module's Play systems and an editor module's Edit overlays, and
`World` cannot tell them apart: filtering is by `ShouldCreate`, never by who registered. The editor gets
no privileged path, which is the property that keeps an editor overlay writable by a game.

**MR4a — a derived type name must strip the elaborated-type keyword, not just the namespace.** MSVC's
`entt::type_name` yields `"class Opaax::Foo"`, and `DeriveTypeLeafName`'s `rfind("::")` removed `class `
**by accident**. The bug surfaced only when a **global-namespace** type first registered (M4 S5's editor
subsystem → `"class QuadBoundsSubsystem"`), because there was no `::` left to strip. Components make this
serious rather than cosmetic: a derived name is the key written into map files, so the first
global-namespace component would have written an unreadable one. Stripped explicitly now, with both
routes covered by a test.

---

## WS — World subsystems (landed M4 S3 2026-07-29; WS7 S4, WS8 S5)

**WS1 — REGISTRY holds candidates; the MANAGER holds instances.** `WorldSubsystemRegistry` (the second
member of `EngineRegistries` — **MR0**) is engine-owned *type metadata*: a flat list of candidate types in
registration order. `WorldSubsystemMgr` — the plain `ISubsystemManager<IWorldSubsystem>` subclass a `World`
already owned — holds the live instances **per world**. One registry, N worlds. This is why
`ISubsystemManager` needed no change: it already separates factories from instances, which is exactly what
a world needs.

**WS2 — `ShouldCreate` is STATIC and OPTIONAL, and that is what makes filtering meaningful.** A candidate
may declare `static bool ShouldCreate(const World&)`; `TWorldSubsystemEntry` detects it with
`if constexpr (requires ...)` and defaults to *always create*. Static because deciding needs no instance —
so a rejected candidate is **never constructed**, which is what makes "an Edit-only overlay does not
*exist* in a Play world" true rather than merely inactive. Optional because most subsystems want "always",
and forcing every one of them to write `return true` is boilerplate. It runs at **every** world creation,
including every PIE start, so it must stay pure and cheap.

**WS3 — `WorldContext` exists because the registration site has nowhere to capture a dependency.**
Editor.md §3 says a subsystem "receives its world and nothing else" and that the *registration site*
captures any app service into the factory. **That is not implementable**: the site is
`WorldSubsystems().Register<T>()`, which takes no arguments and is frozen by **MR1**. Without a context a
subsystem needing `ResourceManager` would reach the locator, which **D3** forbids. So the dependency
arrives by **constructor**, `EditorContext`'s shape one layer down: `{ World& OwningWorld;
ResourceManager& Resources; EngineEventBus& Events; DebugDraw& Debug; }`. Per-world (that is the point —
`OwningWorld` differs, and PIE means two are live), **owned by the `World`** so a subsystem may store
`WorldContext&` for the world's whole life. `EngineRegistries` is deliberately **not** a member: a
registry is type metadata, not a running subsystem's business, and nothing needs it — add a member when
something does.

**WS4 — `std::ref` at the injection point is LOAD-BEARING.** `ISubsystemManager::RegisterSubsystem`
captures ctor args **by value** into the factory lambda, and `StartupAll` **clears `m_Factories`** once
consumed. Passing `WorldContext&` straight through therefore copies the context into a lambda that is then
destroyed, leaving every subsystem's stored reference dangling — a silent use-after-free. `CreateInto`
passes `std::ref(InContext)`, so what gets copied is a *pointer* to the World-owned context. Caught only
because the by-value form also fails to compile (an rvalue will not bind to `WorldContext&`); do not
"simplify" it back. The regression gate is the assertion that a started subsystem's context address
**equals `World::GetContext()`** — comparing `OwningWorld` instead would not discriminate, since freed
memory usually still holds the old value (**L15**).

**WS5 — There is NO `Render` hook for world subsystems.** `WorldManager` overrides `Update`/`FixedUpdate`
and forwards to the **active** world only (a PIE clone and the edit world coexist; exactly one simulates).
`RenderAll` is left unwired even though the base offers it: a subsystem draws by submitting to `DebugDraw`
from its `Update` — immediate mode, drained every frame (**F4**). A second draw path into a frame
`RendererManager` already owns is the thing being avoided.

**WS6 — A world's subsystems shut down through `DestroyWorld`, not through `~World`.**
`World::ShutdownSubsystems()` is **idempotent** (**LC3**) because it is reached two ways:
`WorldManager::DestroyWorld` calls it while every engine sibling a context points at is **still alive**
(the LC-correct moment), and `~World` repeats it as the safety net for `WorldManager::Shutdown`, whose
`m_Worlds.clear()` never goes through `DestroyWorld`. Same **LC1** reasoning as the engine-level phases,
one scope down.

**WS7 — A world subsystem NEVER assumes entities exist at `Startup`; it reads world content from its
first `Update`** (landed M4 S4). True on both creation paths, which is the point of stating it: the
*startup* world's entities are spawned by the host in `PostEngineStartup`, i.e. after `FinishStartup`
(**BO4**), and a *cloned* world is `Capture` → `CreateWorld` → `Instantiate`, so `CreateWorld` starts its
subsystems before the entities land. Instantiating before starting would "fix" the clone at the cost of
making it the only populated-at-`Startup` world in the engine — and "can I read the world in `Startup`?"
would answer *"depends how your world was made"*, which is a far worse contract than a uniform **no**.
`QuadOscillatorSubsystem` is the worked example (baselines on first tick). If something ever genuinely
needs a populated-world moment — physics rebuilding bodies from authoring components — the answer is an
explicit post-instantiate hook (Unreal's `OnWorldBeginPlay` beside `Initialize`), **named here and
deliberately not built**: it has no caller yet.

**WS8 — The PIE tick gate lives in `WorldManager`, and the decision is taken ONCE PER FRAME** (landed
M4 S5). `SetPaused` / `RequestStep` are flags; `Update` — the once-per-frame hook — resolves them into
`m_bTickThisFrame`, and `FixedUpdate` only *reads* that. The ordering is the design, not an
implementation detail: `Engine::Loop` runs `Update` once and `FixedUpdate` 0..N times, so a step that
advanced only `Update` would starve the fixed step and desync a physics world from what the viewport
shows. **A step is a FRAME.** Two consequences worth keeping: a pause arriving between the two hooks
takes effect *next* frame (the current one stays coherent), and the gate is **mode-blind** — an Edit
world pauses exactly like a Play one, because the rule is about the tick, not about what a world is for.
The editor sets a flag and never reaches into the loop.

---

## IN — Input (landed M-Input, 2026-07-31)

**IN1 — One chain, and the ROUTE is the only gate.** `Window → Application → route → InputManager`
(Editor.md D5). The application feeds the engine in `HandleAllInputEvent`; a host that wants to withhold
input **consumes the event before that** (`EditorApplication::OnEvent` returns early when `RouteInput`
says true). So "the editor ate it" and "the engine never saw it" are the same statement, and there is no
second gate downstream to keep in sync. The runtime has no route at all: no editor, no gating, zero cost.

**IN2 — The frame boundary is the HOST LOOP's: `EndFrame()` runs in `RunApplication`, immediately before
`PollEvents` — not in a tick hook, and not inside `Engine::Loop`.** A subsystem `Update()` is too late by
construction: the application polls OS events *before* it calls `Loop`, so the hook would run after the
very events it is meant to precede. But `Loop` is wrong too, and that one shipped before it was caught:
**`Loop` is not the end of the host's frame.** The editor draws its entire UI *after* `Loop` returns, so
clearing there wiped the frame's edges and deltas before any panel could read them — held keys still
worked, which is exactly why it looked fine. Every reader must see the same frame: a game system in
`Update`, an editor panel in the UI pass. The host loop's boundary is the only point that is true for
both.

**IN3 — Edges are LATCHED by the feed, never derived from a previous-frame snapshot.** A key pressed and
released inside one frame leaves both snapshots reading "up", so a comparison **drops the input** — and a
frame is easily long enough for a real keypress once the framerate dips. `OnKeyPressed`/`OnKeyReleased`
set latches that `EndFrame` clears. This is smaller as well as more correct: there is no previous-state
array. Found by a test, not by review.

**IN4 — The feed is IMMEDIATE, not queued through the event bus.** It runs inside `PollEvents`, before
the frame ticks, so a reader mid-route — the editor asking whether Shift is held while handling a key —
gets this frame's truth instead of last frame's copy.

**IN5 — Closing the route RESETS the engine's input, and a reset is not an event.** The OS delivers
releases to whoever has focus, and once the route closes that is someone else; anything held would stay
held forever. `ResetState` **clears** the edge latches rather than filling them, so nothing reports a
release for a press the reader may never have seen. Closings: `WindowLostFocus` (the runtime's only one)
and, in the editor, every open→closed transition of `InputRoute` — which covers PIE pause and stop
without either of them calling input code.

**IN6 — Modifiers are keys.** `IsShiftDown()` reads `LeftShift || RightShift`. There is no modifier
state and no modifier field on the event payload — which is why nothing needs the GLFW `mods` parameter
the window callback discards, and why the editor's reserved shortcuts are bare function keys today.

**IN8 — An EDITOR shortcut cannot read `InputManager`, because the route it lives behind is closed**
(landed M5, 2026-08-03). The engine's input is fed only when `InputRoute` is open; with an Edit world on
screen the route is `ClosedEditMode`, so `RouteInput` consumes every Input-category event and
`InputManager` never sees the keys at all — `IsCtrlDown()` answers false *forever*, precisely where a
`Ctrl+S` wants it. So the editor has **two shortcut mechanisms, and the split is principled**:
- **PIE control (F5–F8) lives in the event route** (`HandleReservedKeys`, D5 step 3). It must fire while
  the *game* owns the keyboard, so it has to sit ahead of the feed.
- **Authoring chords (Ctrl+S, Ctrl+O) live in the UI pass**, via `ImGui::Shortcut`. They only mean
  anything while the *editor* owns the keyboard — which is exactly when ImGui's view of it is the
  authoritative one. ImGui is fed regardless of our route (`ImGui_ImplGlfw_InitForOpenGL(window, true)`
  chains the GLFW callbacks), so this is not a workaround, it is the correct source.
This is **IN6**'s consequence, not a contradiction of it: modifiers are keys, and a mechanism that never
receives keys cannot report modifiers. *(Caught while planning, by asking what `IsCtrlDown` would actually
answer, rather than after building on it — the [[L29]] shape.)*

**IN7 — Gamepad codes are REFUSED, not half-supported.** `EKeyCode` reserves the range, but GLFW exposes
pads by *polling* — a second feed that does not exist. Accepting the code would make `IsKeyDown` answer
"false" forever while looking supported. Same for `KeyTyped`: a Unicode codepoint is text entry and has
no "down" to hold.

---

## WM — World model (World > Level > Map)

Settled with the user 2026-07-28, superseding the retired `Scene` vocabulary (**X4**). Source of concepts:
`Docs/Architectures/EngineArchi.md` — stale in places, see WM5.

**WM1 — Three nouns, one registry.** `World` is the runtime simulation container and **the ECS boundary**:
it owns the single `entt::registry` and the `WorldGuidRegistry`. A `Level` composes Maps and handles
streaming. A `Map` is **pure entity data** — no systems, no runtime ownership — and is therefore **the
serialization unit**. `World { RootLevel (1 map, always mounted, world-scope defaults) + ActiveLevel (N
maps, streamed) }`. Both levels are real Levels holding real Maps; they differ only by a streaming policy,
which is why `Level` does not mean two things.

**WM2 — A Map is a PARTITION of the World's registry, not a container.** One World owns one registry, so
"the entities of map X" is a filter, not a separate store. `EntityMeta::OwnerMap` (`MapId` =
`OpaaxStringID`, interned) is what makes the partition addressable. **Default-invalid means
runtime-spawned** — a bullet no map authored — so filtered capture excludes it *by the rule* rather than by
a special case. Unfiltered capture takes the whole world (the PIE-clone case).

**WM3 — Guid is the only persistent reference.** entt handles are per-registry and never assumed stable
across worlds, so capture→instantiate must preserve GUIDs or every inter-entity reference silently retargets.
`World::CreateEntity` mints a fresh Guid and therefore **cannot** serve instantiate;
`CreateEntityWithGuid` is the restoring entry point, and it refuses an already-live Guid because
`WorldGuidRegistry::Register` *replaces* — a duplicate would evict the original and `FindByGuid` would
start answering the impostor.

**WM4 — A Level's maps are NOT `LoadContext::Acquire`d** (landed M5, 2026-08-03). Both are resources
(`MapResource` / `LevelResource`, both **FailFast**; `.opaaxlevel` = a manifest of map refs, `.opaaxmap` =
entity data; lowercase, matching the shipped `.opaaxproj`). `Acquire` is for *hard* dependencies: it loads
them inline and chains their refcounts to the parent, so acquiring a level's maps would load every one of
them at once and make unloading a single map impossible — the exact opposite of streaming. The manifest
stays **data**. A Map's *textures* will be `Acquire`d; a Level's *maps* never are, and **the asymmetry is
the rule** — `LevelResource::Load` carries that note precisely so the next reader does not "fix" it.

**What M5 actually built, and what it deliberately did not.** The user's scope call: the level layer is a
manifest parsed to a plain `LevelData` plus `LevelLoader`, and there is **no `LevelManager`, no streaming,
and no `Level` object inside `World`**. What landed is the level→map *indirection* that streaming will be
built on top of rather than instead of. `LevelLoader::LoadInto` resolves each map through
`IPaths::AssetToAbsolute`, loads it as a `MapResource` **through the `ResourceManager`** (which is what
gives that type a real caller on every boot rather than only in its own test, [[L23]] — and buys dedup when
two worlds open one map), instantiates it, and drops the ref: nothing needs the parsed data afterwards.
A failed map is counted and skipped — one missing file should cost that map, not the level.
**Trigger for `LevelManager`:** the first thing that genuinely needs to load or unload a map *while the
world is running*.

**WM5 — `EngineArchi.md` is behind the code in two places** (code wins, X4): it puts `MapId ownerMap` in
`EntityMeta` as though it were already there (M3 S2 actually added it), and it makes `GuidRegistry` global
(`Guid → World*, entt::entity`) where the code made it **per-World** — entt handles are only valid inside
their own registry, so a Guid resolves through its world and never crosses worlds.

**WM6 — A world CLONE is a snapshot round trip, so it copies exactly what the ComponentRegistry knows**
(landed M4 S4). `WorldManager::CloneWorld(source, mode)` = `Capture` (**unfiltered** — a clone that
dropped runtime-spawned entities would start out already diverged, **WM2**) → `CreateWorld(name, mode)` →
`Instantiate`. Three properties come from that shape rather than from clone-specific code:
the clone keeps the source's **Guids** (**WM3**) but gets its **own** world Guid; its **mode is the
caller's argument, never inherited** — going through the ordinary `CreateWorld` is what makes its
subsystem set follow that mode (**WS2**); and a component type the registry does not know is **not
carried** (it has no stable name to be written under — a registry gap, not a clone bug). The source is
read-only throughout and stays alive, which is the whole restore mechanism: Stop re-activates it and
destroys the clone, undoing nothing. No `MapId` filter here — the filtered form is "save this map" (M5).

**WM7 — A world's NAME is display text, so it stays an `OpaaxString`; interning is for KEYS**
(settled 2026-08-05, closing a `//Todo: OpaaxStringID` on `CreateWorld`). `OpaaxStringID` buys one thing:
O(1) integer compare on a key. Nothing compares or looks up a world by name — every `World::GetName()` in
the tree is a log arg, the toolbar's display string, or the copy `CloneWorld` hands the clone — and no
`LevelFile::Save` exists, so it is never persisted either. Converting would make the *only* live path
slower (`ToString()` = `shared_lock` + an `OpaaxString` copy out of the pool, where `.CStr()` is a
pointer) and would intern unbounded author content — the pool never evicts, and a world's name is the
level's `name` key or a file stem (**BO4b**). The tree already draws this line correctly: the two
registries intern *because* they do `GetName() == InName`, `MapId` interns *because* `EntityMeta::OwnerMap`
is compared per entity (**WM2**), and `EntityMeta::Name` — display, like this one — does not.
**Trigger to revisit:** the first thing that resolves a world BY name (a `FindWorld(name)`, or persisted
editor session state naming one).

---

## MP — Map/Level file layer (landed M5, 2026-08-03)

Four layers, each knowing only its neighbours — which is why the M3 snapshot core needed **no change at
all** to gain persistence:
`World` ⇄ `MapSerializer`/`MapFactory` ⇄ `MapData` ⇄ `MapJson` ⇄ `MapFile` ⇄ a file.

**MP1 — Everything interned is written as its STRING, and that is why this layer exists.** An
`OpaaxStringID` is an intern-table *index*, so a map that wrote one as a number would mean something
different on the next run (**WM2**). `MapJson` is the only place that conversion happens.
- The **invalid** id is written as `""`, not as the `"None"` its `ToString()` answers. The round trip
  would in fact survive `"None"` — `OpaaxStringIDPool` reserves index 0 for it, so re-interning that text
  yields the invalid id back — but the file would claim a runtime-spawned bullet belongs to a map called
  None. A map file is read by humans; `""` is the encoding that does not lie. *(The original plan
  overstated this as data loss; the test that asserted the round trip broke is what corrected it.)*
- A `Guid` is 32 lowercase hex chars, High then Low, no dashes (`Guid::ToString`/`FromString`, out-of-line
  in the DLL). Big-endian nibbles, so lexicographic order over the text matches numeric order over the
  value. `FromString` parses into **locals** and only then writes its output: a half-written Guid is a
  *different identity*, not a rejected one, and the caller could not tell.

**MP2 — Entities are SORTED BY GUID on write, and two consumers depend on it.** entt's view order is
storage order, so an unsorted write would reshuffle the whole file every time an entity was destroyed
(a map file lives in git), **and** the editor's derived dirty check — which compares serialized text —
would report a world nobody edited. Components are a json **object** keyed by authoring name, so a
duplicate type on one entity is unrepresentable rather than merely unlikely. Nothing reads a map in order.

**MP3 — Reading a map file is TOLERANT AND TOTAL; refusing leaves the caller's data untouched.** nlohmann
throws on a type mismatch, so every read is guarded — a hand-edited file is an ordinary input. A missing
field defaults, an unknown field is ignored, and an entity with **no usable guid is SKIPPED** rather than
given a fresh one (minting an identity silently retargets every reference that pointed at it, **WM3**). A
`version` NEWER than this build is refused rather than half-read. Every failure path down the stack leaves
`OutData` alone, so a map that fails to read cannot half-replace one already held and then be written back
over the original.

**MP4 — Saving is NOT a `ResourceManager` capability.** That API is frozen at
Load/Resolve/Pin/FlushAll/Update, and its own header says every future capability is a separate system
*consuming* it. `MapFile::Save` is that system, and it sits beside `MapFile::Load` in one unit because the
two are halves of one format contract — splitting them across files is how a writer and a reader drift.

**MP5 — The editor's dirty flag is DERIVED, never tracked.** Capture the world filtered by the document's
`MapId`, serialize, compare against the text last written or read.
- **There is nothing to hook.** Every edit goes through a registered drawer whose contract is
  `bool(Entity&)` meaning *was it drawn*, not *was it changed*. Tracking would mean changing that contract
  and every drawer with it, a game's included.
- It gets the interesting case right: drag a quad away and back and a **flag** says dirty while the file is
  already correct; the comparison says clean, which is true.
- It cannot drift — a derived answer has no second copy of the state to fall out of sync with.
- **It must be throttled, not per-frame** (4×/s, cached in `EditorService` where the frame clock is, so
  `IsDirty` stays pure). Built per-frame first, which also put ~1700 identical lines in a 10-second log —
  hence `MapSerializer::Capture` logging at **Trace**: it is a pure transformation with several callers,
  and an Info belongs to things that happen *to* something (`MapFile` Save/Load, `MapFactory` Instantiate).

**MP6 — Adopting a map checks ROUND-TRIP STABILITY and logs it either way.** The milestone rests on
world→file→world→file being a fixed point; when it is not, every Save rewrites the map with churn around
the one value that changed — and that is silent otherwise, because the map still loads and the world still
looks right. `EditorMapDocument::AdoptExisting` compares its fresh baseline against the bytes on disk and
says *"Round trip is stable"* or warns. It is the one gate on the whole layer that needs no human.

---

## X — Old/new coexistence

**X1 — The old world is quarantined in `Engine/Source/Legacy/` and dropped from the build (2026-07-19).**
`CoreEngineApp`, every `*Old` type, `EventOld/`, the old `Scene/`/`World/`/`ECS/`/`Physics/` trees, the old
editor (`Engine/Source/Editor/`), the old subsystems (`Core/Systems/{GameSubsystem,MoverSubsystem,
PhysicsSubsystem,Movement}`), the old renderer (`Camera/` controllers, `Pass/`, `Systems/WorldRenderSystem`,
`RenderSubsystem`), the static `RenderCommand` path, and **`Core/OpaaxPath`** (the old static path system —
five mutable statics, i.e. an **I1** violation; superseded by `IPaths`/`ResolveProjectLayout`, quarantined
2026-07-28) — all live under `Engine/Source/Legacy/`, **NOT globbed by the engine DLL** (compiled = zero). Their old-world tests live in `Engine/Tests/Legacy/`. **Do not add new
dependencies on any of it, and do not re-glob `Legacy/`.** It will be deleted; anything you hang off it dies with
it. *(Exception: `Core/Systems/Subsystem.h` — `ISubsystem`/`ISubsystemManager` — stayed LIVE; it is the base of
the new `EngineSubsystemBase`.)* Next: Gregory-layer the live remainder (plan `~/.claude/plans/inherited-orbiting-clover.md` — the
auto-named plans live in the USER-level `.claude/plans/`, not the repo's).
**X2 — New systems get collision-proof identities up front.** When old and new coexist, the *new* type gets
a scoped `enum class` / distinct name — never rely on include order or forward-decl tricks to avoid a
clash. Two unscoped enums sharing enumerator names collide the moment one TU needs both (**L4**).
**X3 — A rename to `*Old` carries through to enumerators and consumers**, not just the type name.
**X4 — New code takes the LIVE vocabulary, never the quarantined one** (M2d, 2026-07-27). `Asset` belongs
to the retired `Legacy/Assets` world (`IAsset`, `AssetRegistry`, `AssetManifest`); the live system is
`CResource` / `ResourceManager`. So the editor's file browser is `ResourceBrowserPanel` on a
`ResourceTypes()` route, not `AssetBrowser`/`AssetTypes`. Before naming anything, grep the term: if
`Legacy/` owns it, the name is taken — reusing it makes every future search ambiguous and quietly
suggests a lineage the new code does not have. Older planning docs predate such renames; the CODE is the
vocabulary of record.

---

## PL — Placing a new system (decision procedure)

1. **Name its Gregory layer** (**I4**). Passive facility ⇒ app service. Ticks / owns game concepts ⇒ engine subsystem.
2. **App service?** interface + `OPAAX_SERVICE_TYPE` (out-of-line `.cpp` — **I2**), `Null()`/`IsNull()`
   (**I3**), provided in `Bootstrap` at the right dependency point (**BO**), torn down reverse.
3. **Engine subsystem?** `EngineSubsystemBase` + `OPAAX_SUBSYSTEM_TYPE`, registered with `EngineSubsystemMgr`,
   implement `Startup`/`TearDown`/`Shutdown` honoring **LC**; borrow siblings via **F3**.
4. **Ownership** follows lifetime (**I5**); **no new static** (**I1**).
5. **Simplicity gate:** does a mid-sized indie engine really need this now? Simple > clever
   (project value #1). If a premise balloons past its sketch, STOP and surface a scoped fork with a
   recommendation (**L3**) — don't build the expensive version unseen.

---

## CH — Change protocol (how we work; full text in CLAUDE.md)

- **Build truth:** `./build.bat` ends with `OPAAX_BUILD_OK` / `OPAAX_BUILD_FAIL` and a real exit code.
  Grep the marker; never trust "looks done." `build.bat fast [target]` = seconds-long incremental check.
- **Forks up front:** surface the 2–3 real scope/premise forks *during planning*, with a recommendation,
  decide once, then build the decided version. Most back-and-forth in `lessons.md` is a fork found mid-build.
- **Gates:** every milestone step has a demo gate + unit-test gate. A red build right after your change is
  not proof your change broke it — `git status` shows your true blast radius (**L5**).
- **Perf is a gate, not a vibe:** hot-path systems (bullets, sprites, bodies) carry a *loose* perf budget.
  Bench cases live in `Engine/Tests/Perf/` as a doctest suite `"perf"` marked `skip()` — they never run in
  `build.bat test`/CI (unit baseline stays 106/426). Run RELEASE via **`./build.bat bench`**. Soft gate:
  budgets catch algorithmic/allocation regressions (O(n²), per-op alloc), NOT micro-noise; watch the printed
  ns/op yourself for ~2× drift. Helper: `Engine/Tests/Perf/PerfBench.h` (median-of-epochs, homegrown — a
  loose gate doesn't need nanobench). Add a case → add its path to `Engine/Tests/CMakeLists.txt` (explicit list).

---

## Pointers

- **Post-mortems / rules:** `.claude/lessons.md` (L1–**L32**).
- **Live session state:** `.claude/CLAUDE.local.md` (current milestone, standing decisions).
- **Working checklist:** `.claude/task/todo.md`.
- **Ground truth for engine design:** `.claude/data/` — *Game Engine Architecture* (Gregory). Prefer it over
  generic advice.
