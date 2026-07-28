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
- The "duplicates per module" hazard (**L4**) bites only a tag that is **not** dll-exported: a header-only
  template static, or a subsystem living in a **static lib / game module** (M4). Such a type must go
  out-of-line (service shape) or be exported. If `GetSubsystem<T>()` across the boundary ever returns null,
  this is the first suspect.
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
- **Corollary (M3): `OPAAX_API` instantiates every IMPLICITLY-declared member**, so an exported class
  holding a move-only member (`TDynArray<UniquePtr<T>>`) fails to compile on its implicit *copy*-assign
  (C2280) even though nothing ever copies one. Declaring copy/move `= delete` is therefore **required**,
  not hygiene — the shape `World` already uses, now also `ComponentRegistry`.

**I8 — A component is defined by a CONCEPT, not a base class** (landed M3, 2026-07-28). `CComponent`
(`World/Components/ComponentConcept.hpp`) requires nlohmann `to_json`/`from_json` by ADL — the same
concept-over-base-class shape as `CResource`, and the only shape available: **entt stores components by
value**, which is why `ComponentBase` is empty and non-virtual, so `Save`/`Load` cannot be member virtuals
the way the retired `Legacy/ECS/ComponentRegistry` did it. A game component costs one
`NLOHMANN_DEFINE_TYPE_INTRUSIVE` and one `Components().Register<T>()`; the engine names it nowhere.
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
`Shutdown` is idempotent. The `IEngine` service is provided **last** in Bootstrap, so it tears down
**first** — before the window/GPU context dies.

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
Engine().Startup()      → every subsystem constructed and started. NO WORLD EXISTS.
RegisterModules(...)    → components -> ComponentRegistry, world subsystems -> its registry
OnModulesRegistered()   → editor extensions; editor seals its own registries
CreateStartupWorld()    → the first CreateWorld SEALS ComponentRegistry, then makes the world
```

`WorldManager::Startup` **must not create a world.** It used to spin up a default "Main" so there was
"always a render target" — dead weight, because every consumer already handles no-active-world
(`RendererManager::Render` guards; `HierarchyPanel` renders "No active world."). What it *did* do was make
the boot order unfixable: the first `CreateWorld` seals `ComponentRegistry`, so a world born inside
subsystem startup locked the registry before any game module could reach it.

**The world comes from config, not from code**: `OpaaxApplication::CreateStartupWorld()` (an SE seam) reads
`IProjectManager::StartupLevel()` — the project's `.opaaxproj`, key `startupLevel`, with `startupScene` /
`defaultScene` as Scene-era fallbacks (**X4**). Falls back to "Main". *M3 only NAMES the world; loading that
level's maps needs the M5 file layer.* Hosts override to open something else — the editor will want the
last-opened map, not the game's startup level.

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

**F3 — A subsystem needing a sibling mid-boot resolves from the manager.** During its own `Startup`, a
subsystem reaches a sibling via `m_Subsystems.GetSubsystem<T>()` (the manager's create-pass populates the
list before any `Startup` runs) — **never** via a lazy accessor that can re-enter the owner's boot. Doing
the latter causes the per-frame re-init loop of **L6**.

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
| `CreateStartupWorld()` | in `EngineStartup`, **after `OnModulesRegistered`** — the last step of boot (BO4) | create + activate the world the app starts in, named from `IProjectManager::StartupLevel()`. Base impl is real, not a no-op; override to open something else. This is the **first `CreateWorld`**, so it seals the registries |
| `PostEngineStartup()` | end of `EngineStartup` | after subsystems start **and the startup world exists** (editor inits `EditorService`; Sandbox populates the world) |
| `TickFrame()` | per loop iter | base = `Engine().Loop()`; editor wraps it UI-begin → Loop → UI-end |
| `OnEvent(event)` | window callback, per event | base = app sink (close/resize→bus); `EditorApplication` overrides → `EditorService::RouteInput` first (S11), so the editor sees events before the bus |

---

## MR — Module registration (D9)

A game module is an **`IRuntimeModule`** (`Application/IRuntimeModule.h`); its `OnRegister` registers
**into** a `ModuleRegistrar`, invoked by the host's `RegisterModules` seam before any world exists:
```cpp
InRegistrar.Components().Register<TransformComponent>();      // → ComponentRegistry v2 (M3)
InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>(); // → WorldSubsystemRegistry (M4)
```
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
**MR3 — One module shape.** Runtime and editor modules share a marker base **`IModule`**
(`Application/IModule.h`): `IRuntimeModule : IModule` (`OnRegister(ModuleRegistrar&)`) and
`IEditorModule : IModule` (`OnRegister(EditorExtensionRegistrar&)`). `OnRegister` stays on each derived
interface — the two register into *different* registrars, so it can't sit on the base. All three are
header-only pure interfaces, **no `OPAAX_API`** (no exported symbols / shared state / identity tag — the
opposite end of the axis from **I2**). A host invokes a module as a throwaway instance
(`SandboxModule().OnRegister(reg)`), symmetric across runtime and editor.

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

**WM4 — A Level's maps are NOT `LoadContext::Acquire`d.** Both are resources (`.opaaxlevel` = a manifest of
map refs + stream rules; `.opaaxmap` = entity data; lowercase, matching the shipped `.opaaxproj`). `Acquire`
is for *hard* dependencies: it loads them inline and chains their refcounts to the parent, so acquiring a
level's maps would load every one of them at once and make unloading a single map impossible — the exact
opposite of streaming. The manifest stays **data**; `LevelManager` loads maps per stream request and holds
those refs itself. A Map's *textures* are `Acquire`; a Level's *maps* are not.

**WM5 — `EngineArchi.md` is behind the code in two places** (code wins, X4): it puts `MapId ownerMap` in
`EntityMeta` as though it were already there (M3 S2 actually added it), and it makes `GuidRegistry` global
(`Guid → World*, entt::entity`) where the code made it **per-World** — entt handles are only valid inside
their own registry, so a Guid resolves through its world and never crosses worlds.

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
the new `EngineSubsystemBase`.)* Next: Gregory-layer the live remainder (plan `inherited-orbiting-clover.md`).
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

- **Post-mortems / rules:** `.claude/lessons.md` (L1–L8).
- **Live session state:** `.claude/CLAUDE.local.md` (current milestone, standing decisions).
- **Working checklist:** `.claude/task/todo.md`.
- **Ground truth for engine design:** `.claude/data/` — *Game Engine Architecture* (Gregory). Prefer it over
  generic advice.
