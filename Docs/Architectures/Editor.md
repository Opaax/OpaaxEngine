# Opaax Editor — Architecture & Plan

**Status:** Accepted — **v3 (frozen)**. Adds the boot/registration flow, the World Subsystem model, and the game-module restructuring. Supersedes v2.
**Branch:** `refresh_engine`
**Date:** 2026-07-16
**Scope:** Editor application built on the new App/Service architecture, the contract by which a game plugs into it, and the World Subsystem model both depend on. Supersedes everything under `Engine/Source/Editor/**` and the `CoreEngineApp::LaunchEditor()` path.

---

## 1. Model

One engine. One editor **library**. The editor is not an engine variant, and a game editor is not a fork — both are *host compositions*. There is one `GameEditor.exe` per game, and it is ~10 lines long.

The whole codebase follows **one lifecycle pattern at three scopes**:

| Scope | Container | Lifetime |
|---|---|---|
| Application | `AppServiceLocator` (services) | the process |
| Engine | `EngineSubsystemMgr` | engine startup → shutdown |
| World | `WorldSubsystemMgr` (§3) | one world |

```
Opaax/
├── Engine/               → lib. Zero editor knowledge. Ever.
├── OpaaxEditorLib/       → lib. The whole editor: EditorService, panels,
│                           UI backends, extension registries. Links Engine.
│
└── <Game>/                       (Sandbox is the first instantiation, M0)
    ├── Module/           → static lib: components, maps, world subsystems,
    │                       RegisterModule()          → linked by BOTH exes
    ├── Editor/           → the game's editor extensions: custom drawers,
    │                       panels, asset actions, IEditorModule
    │                       → compiled ONLY into GameEditor.exe
    ├── Runtime/          → main: GameApp : OpaaxApplication        → Game.exe
    └── EditorMain/       → main: GameEditorApp : EditorApplication → GameEditor.exe
```

Link lines:

- `Game.exe` = Engine + Game/Module
- `GameEditor.exe` = Engine + OpaaxEditorLib + Game/Module + Game/Editor
- `SandboxEditor.exe` (dev host during M0–M2) = Engine + OpaaxEditorLib + Sandbox/Module

Dependency rules — **the arrow never goes up**:

| Source set | May include |
|---|---|
| `Engine` | nothing above it |
| `OpaaxEditorLib` | Engine |
| `Game/Module` | Engine only — **never an editor header** (or ImGui leaks into Game.exe) |
| `Game/Editor` | Engine, OpaaxEditorLib, Game/Module |
| mains | everything their exe links |

- The app subclass *composes*; the module *is the game*. Anything in the app subclass exists in one exe only; anything in the module exists in both.
- Ship binaries (Steam/EGS) never link editor code. No ImGui in runtime targets.
- Build config (Debug/Release/Dist) and target (Editor/Runtime) are **orthogonal axes**. The editor must build in Release; the runtime must build in Debug. The CMake coupling `OPAAX_EDITOR_SUPPORT → non-Release only` is removed.
- Target composition, zero ifdefs — the philosophy of D1/D4 applied to the game layer.

---

## 2. Boot & registration flow

The order **is** the design. The host guarantees it; nothing else may.

```
Bootstrap
 └─ BootEngine             → provides the IEngine service (app services incl. the
                             project's StartupLevel are loaded here)
EngineStartup                                     — infrastructure, then content
 ├─ Engine().Startup()     → every subsystem constructed and started.
 │                           NO WORLD EXISTS. Engine natives are registered.
 ├─ RegisterModules (seam) → game module: components → ComponentRegistry,
 │                           world subsystems → WorldSubsystemRegistry
 ├─ OnModulesRegistered    → editor module adds its Edit-world candidates through
 │                           the SAME registry, then seals its own registries
 └─ CreateStartupWorld()   → the first CreateWorld SEALS the registries, then
                             creates + activates the world named by the project's
                             startupLevel. Host seam: override to open another.

CreateWorld(name, mode)                        DestroyWorld(world)
 └─ per candidate:                              └─ broadcast WorldDestroying
    ShouldCreate(world)? → factory → Initialize     (world still whole)
    (registration order)                        └─ Deinitialize, reverse order
 └─ tick-list built once                        └─ destruction
 └─ broadcast WorldCreated
    (world fully formed)
```

Three sources — engine natives, game module, editor module — one registry, one mechanism.

**Why the world comes last (M3).** `WorldManager::Startup` used to create a default "Main" world, so the
first `CreateWorld` — and therefore the seal — happened *inside* subsystem startup, before any module could
register. That is a subsystem doing **content** work during **infrastructure** boot. Creating the world is
now the host's last step, driven by config, and the ordering problem disappears rather than being worked
around. Nothing needed the early world: every consumer already handles "no active world".

This diagram described the intended order for three milestones before the code could honour it — planning
artifacts age (`.claude/lessons.md` L19, L22).

---

## 3. The World Subsystem model

A `World` owns a `WorldSubsystemMgr`, mirroring `EngineSubsystemMgr` at world scope. The Unreal-style contract, without the Unreal-style discovery:

```cpp
// Candidate contract as BUILT in M4 S3 — no reflection, everything explicit.
class WaveSpawnSubsystem final : public WorldSubsystemBase
{
public:
    OPAAX_SUBSYSTEM_TYPE(WaveSpawnSubsystem)

    // OPTIONAL. Omit it and the subsystem is created in every world (the common case pays
    // no boilerplate). Static, so no instance is needed to decide.
    static bool ShouldCreate(const World& InWorld)
    { return InWorld.GetMode() == EWorldMode::Play; }

    explicit WaveSpawnSubsystem(WorldContext& InContext);  // <- the injection point

    bool Startup() override;    // ISubsystem's own names, not Initialize/Deinitialize
    void Update(double InDt) override;
    void Shutdown() override;   // reverse order
};

InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>();
// Register<T>() bakes ShouldCreate + the factory into a type-erased entry:
// no virtual for the predicate, no instance needed to decide, no static-init.
```

**Corrected in M4 S3:** the sketch above said `Initialize(World&)` / `Deinitialize()`. The real thing
reuses `ISubsystem`'s existing `Startup`/`Shutdown` — a world subsystem IS an `ISubsystem`, and inventing a
parallel pair of lifecycle names would have meant a second vocabulary for the same two moments. The world
arrives through the constructor instead, as `WorldContext::OwningWorld`.

### The registry
- Owned by `Engine` as one member of `EngineRegistries` (**MR0**), and fed by three sources BEFORE the first world exists (§2). **No reflection, no static-init discovery** — candidates are handed over as an explicit list; template capture at registration replaces UClass.
- `WorldManager` is the only consumer: it is where worlds are created and destroyed, so it is where subsystem lifetimes are decided. It **borrows** the registries; it does not own them.
- **The registry holds CANDIDATES, the per-world `WorldSubsystemMgr` holds INSTANCES.** One registry serves N worlds. `WorldSubsystemMgr` needed no code of its own — `ISubsystemManager` already separates factories from instances, which is exactly a world's requirement.

### The three locks
- **L1 — Sealed at first `CreateWorld`.** Late registration = loud assert. A subsystem registered after a world exists would silently never run in it — the classic three-weeks-later bug. Forbidden, not handled.
- **L2 — Mode is a `CreateWorld` parameter, immutable.** `ShouldCreate` reads the mode, so the mode must exist before any subsystem does. Immutability kills the entire "what happens to subsystems when the mode flips" problem: changing mode = creating another world. Coherent with PIE-by-clone.
- **L3 — Two-phase events.** `WorldCreated` broadcasts *after* init (listeners see a fully formed world); `WorldDestroying` broadcasts *before* deinit (listeners react while everything is alive). Same philosophy as `IEngine::TearDown/Shutdown` — one lifecycle doctrine across the codebase.

### Mechanics
- **`ShouldCreate` absorbs all mode filtering.** Play-only gameplay (`Mode == Play`), Edit-only overlays (`Mode == Edit`), always-on infrastructure like transform/hierarchy propagation (`return true`). One question, asked once, at world creation. `ShouldCreate` must be pure and cheap — no side effects.
- **Tick is opt-in, resolved at Init.** The manager builds its tick-list once at world creation — never a per-frame virtual "do you want to tick?". Two hooks, matching the engine's fixed-timestep loop: `Update(dt)` and `FixedUpdate(fdt)`. **No `Render` hook** — a subsystem that wants to draw uses the engine `DebugDraw` API during its update; the renderer flushes.
- **Ordering convention.** Registration order = init order = tick order; reverse for deinit. Same rule as the locator and `EngineSubsystemMgr`. No dependency graph at this scale.
- **Injection rule — REWRITTEN in M4 S3; the original was not implementable.** It said a subsystem "receives *its world* and nothing else", and that the *registration site* captures any needed app service into the factory. But the registration site is `WorldSubsystems().Register<T>()`, which **takes no arguments** and is frozen by MR1 — there is nowhere to capture anything, so a subsystem needing `ResourceManager` would have had to reach the locator, which D3 forbids. The dependency therefore arrives **by constructor**, in a `WorldContext` — `EditorContext`'s own shape one layer down: `{ World& OwningWorld; ResourceManager& Resources; EngineEventBus& Events; DebugDraw& Debug; }`. Resolved once by `WorldManager::Startup`, composed per world by `CreateWorld`, owned by the `World` so the reference stays valid for its whole life. Nothing downstream sees the locator. Game subsystems should still live almost exclusively off their world's ECS data — the context is for the cases that genuinely cannot.

### Consequence: PIE becomes mechanical
Creating the PIE world walks the candidate list → `Play` subsystems are born → runtime state is rebuilt from authoring data. D7's *"rebuilt by system startup"* now has an exact address. Stop → `Shutdown` in reverse; runtime state dies with the world. Zero special cases.

**Corrected in M4 S4 — that rebuild happens on the first `Update`, NOT in `Startup`.** `CloneWorld` is `Capture` → `CreateWorld` → `Instantiate`, and `CreateWorld` starts the subsystems, so at `Startup` the clone is still **empty**; the entities land immediately after. This is not an ordering accident to be fixed — the *startup* world has the same shape (BO4 has the host spawn entities after `FinishStartup`), so the rule holds uniformly: **a world subsystem never assumes entities exist at `Startup`** (ARCHITECTURE.md **WS7**). The alternative — instantiate first, then start — would make a cloned world populated at `Startup` while a fresh one is empty, so "can I read the world in `Startup`?" would answer *"depends how your world was made"*. If a subsystem ever genuinely needs a populated-world moment (physics rebuilding bodies), the answer is an explicit post-instantiate hook, Unreal's `OnWorldBeginPlay` beside `Initialize` — named here, deliberately not built, because nothing calls it yet.

---

## 4. Decisions

### D1 — The editor is an application service; the host has three seams
`IEditorService : IAppService`, implemented in `OpaaxEditorLib`, provided **only** by `EditorApplication::OnProvideServices()`.
The seams in `OpaaxApplication` are the sole extension points:

- `OnProvideServices(AppServiceLocator&)` — end of `Bootstrap()`, derived hosts add their services.
- `OnRegisterModules(ModuleRegistrar&)` — between `Bootstrap` and `EngineStartup`. Registries exist (post-`BootEngine`), no world exists yet (pre-`Startup`). Modules ≠ services — hence a distinct seam.
- `TickFrame()` — per-frame body, default `Engine().Loop()`. Editor override: `UI begin → engine frame → UI end`.

Runtime behavior is byte-for-byte unchanged when these are not overridden.

### D2 — Engine output contract
One window, owned by `WindowManager`. The editor never creates a second one.

The engine renders into a **RenderTarget it owns**. Rule: *whoever owns the frame owns the present.*

| | Runtime | Editor |
|---|---|---|
| Engine renders to | its RenderTarget | its RenderTarget |
| Backbuffer filled by | host (blit target → backbuffer) | ImGui (engine output = texture in viewport panel) |
| Present / SwapBuffers | App loop, after `TickFrame()` | App loop, after `TickFrame()` |
| Engine resolution driven by | window size | **viewport panel size** (resize is inverted) |

`IEngine` exposes its output view (the existing `GetViewportImage` RHI bridge is the model) and accepts a resize request. `SwapBuffers` never lives inside `Engine::Render`.

### D3 — Zero statics beyond the locator
`OpaaxApplication::m_Services` is the single accepted global. Rule: **only composition roots touch the locator.**

- `EditorService` is the editor's composition root. At startup it resolves its dependencies once and builds an `EditorContext` — a flat struct of references: `IEngine&, WorldManager&, ResourceManager&, AssetRegistry&, EditorState&, EditorEventBus&` — injected **by constructor** into every panel and drawer. A panel never sees the locator, never a static.
- Old static registries become `EditorService` members, filled by **explicit registration**. No static-init automagic (DLL/static-template hazard, see `IAppService.h`).
- Applies identically to game extensions and to world subsystems (§3 injection rule).

### D4 — ImGui lives in OpaaxEditorLib
ImGui and the `IEditorUIBackend` implementations (GL/VK) belong to `OpaaxEditorLib`. `Engine/Source/Editor/**` stops being globbed into `OpaaxEngine` (`Engine/CMakeLists.txt:56`).

### D5 — Input is a route, not an engine context system
The engine stays dumb: `InputManager` is *fed or not fed*. All routing policy lives in `EditorService`. Per-event decision order:

1. ImGui `WantCaptureMouse/Keyboard` → the UI eats it. **With one exemption, learned the hard way (M-Input S2):** the Viewport is an ImGui window like any other, so `WantCaptureMouse` is true the entire time the pointer is over it — taken at face value, a game running inside the editor can never receive a click, a drag or the wheel. **Over a hovered viewport, mouse capture does not apply**: ImGui owns the pointer over the UI, the game owns it over the surface it is being played on. Keyboard is *not* exempted — `WantCaptureKeyboard` only goes true for a text field, and a field with the keyboard must always win.
2. Viewport neither hovered nor focused → nothing passes beyond the editor. **LIVE since M-Input S2.** Hovered *or* focused, so dragging out of the panel mid-gesture does not cut the input off. ImGui can only answer during `Draw()`, so the flags are one frame old — the same lag the viewport's deferred resize already lives with.
3. Reserved editor keys (Esc, play/pause shortcuts) → the editor eats them. **LIVE since M4 S5:** `F5` Play · `F6` Pause/Resume · `F7` Step · `F8` Stop, non-repeat presses only, checked *after* step 1 so a shortcut can never fire while a text field owns the keyboard. Bare function keys rather than chords because the `KeyPressed` payload carries **no modifier state** — `Ctrl+P` is not expressible today, and giving it one belongs to M-Input, not to a PIE slice. Esc is still unassigned (it will want to mean "deselect" too).
4. Remainder, by active world mode: `Edit` → editor tools (camera, selection — editor systems, never `InputManager`); `Play` → `InputManager` → game. **LIVE since M-Input S2**, plus a fourth condition the original list did not anticipate: a **paused** PIE session is closed too, or a frozen world would bank keystrokes to replay on resume.

Steps 2 and 4 live in one object, **`InputRoute`** — `RouteInput` gates on it and the Input panel displays it, so the behaviour and the readout cannot drift. It is evaluated **once per frame** in `BeginFrame`, not inside `RouteInput`: a rule evaluated only when an event arrives cannot notice that input *stopped*, and that is exactly the case that must trigger the reset below. *(Corrected from the original sketch: the game is fed **directly**, not "via the engine event bus". The bus is queued and flushed at the top of `Loop`, which would answer a mid-route question with last frame's state — see ARCHITECTURE.md **IN4**.)*

Runtime: the chain does not exist. Window → bus, zero cost.

**Engine-side contract:** `InputManager::ResetState()` — release-all, called whenever the route closes (focus lost, pause, PIE stop). **LIVE since M-Input.** Without it: held key = stuck key. Two details the original line did not have: it fires on the open→closed *transition*, so PIE pause and stop get it without either calling input code; and it **clears** the edge latches rather than filling them, because a reset is not an event — reporting a release for a press the game never saw is its own bug (ARCHITECTURE.md **IN5**).

**Still not built, and named rather than implied:** action maps (a game-layer concept, D5's own words), gamepad (GLFW polls pads — a second feed), world-space mouse (needs the viewport rect *and* the camera), and the editor camera that would give D5's `Edit` branch something to do. Today that branch consumes input and drops it, which is correct while no editor tool exists.

Gameplay input contexts (action maps) are a *game-layer* concept — routing decides *who is fed*, game contexts decide *how the game interprets*. Not conflated.

### D6 — Worlds, modes, PIE
`WorldManager` owns every world. The editor *commands* it (`CreateWorld`, `DestroyWorld`, `SetActiveWorld`, `CloneWorld`) and never stores a world itself.

- `EWorldMode { Edit, Play }` is an **immutable `CreateWorld` parameter** (lock L2, §3).
- Which systems a world runs is decided entirely by `ShouldCreate` at creation (§3): physics and gameplay exist only in `Play` worlds; transform/hierarchy propagation in both; editor overlays only in `Edit` worlds. Render draws the active world regardless of mode.

**PIE = capture + re-instantiate. Not a registry memcpy.**

- **Play:** `MapSerializer::Capture` takes the edit world → `MapData` → `MapFactory::Instantiate` builds it into a fresh world created with mode `Play` → its `Play` subsystems rebuild runtime state → `SetActiveWorld(PIE)`. The edit world stays alive, dormant, untouched.
- **Stop:** `SetActiveWorld(Edit)` → `DestroyWorld(PIE)` (subsystems shut down in reverse, runtime state dies with the world). Restore is free — the entire point of clone-on-play.

**LIVE since M4 S4/S5.** The whole sequence is `WorldManager::CloneWorld(source, mode)` — one call, so the editor composes nothing — driven by **`PlayInEditor`**, the state machine `EditorService` owns and `EditorContext` carries. It exists as its own type because *two* front-ends drive it: the `Play Controls` panel's buttons and D5's reserved keys. Two details that are load-bearing rather than stylistic:

- **`SetActiveWorld(edit)` strictly BEFORE `DestroyWorld(clone)`** on Stop — `DestroyWorld` clears the active slot when it is destroying the active world, so the other order leaves the editor with no world for the rest of the frame.
- **Pause/Step are a `WorldManager` tick gate** (ARCHITECTURE.md **WS8**), not something the editor does to the frame loop: the editor sets a flag, `Update` resolves it once per frame, and `FixedUpdate` inherits that decision so a step advances a whole frame rather than desyncing the fixed step.

**A world switch is an EVENT, not something panels poll.** PIE swaps the active world twice per session, and every panel already re-reads `GetActiveWorld()` each frame — what they cannot do is *notice* the swap and drop what they cached. So `IEditorPanel::OnActiveWorldChanged(old, new)` (default no-op) is fanned out by `EditorService` from its `WorldManager` subscription, **after** the selection has been retargeted so no panel observes a stale one. The selection itself **follows the entity by `Guid`** into the clone and back — a clone preserves GUIDs (**WM3**), so the M3 snapshot guarantee is what makes "your selection survives Play" a five-line consequence instead of a feature. It clears when the Guid has no counterpart, which is also what kills the old dangling-`Entity` hazard.
- Same code path as runtime map loading: **PIE = save-to-memory + normal load.** Only the origin of the `MapData` differs.
- Capture with **no `MapId` filter** — the clone must include runtime-spawned entities, or it diverges from the world it copied. The filtered form is for "save this map" (M5), not for cloning.

`// PERF:` if profiling ever shows PIE start too slow on large maps, a binary snapshot fast-path of authoring storages can live behind the same `CloneWorld` API. The contract does not change; do not build it before the profile asks.

### D7 — Component rule: authoring vs runtime
Every component classifies **on arrival** in the new world:

| | Authoring | Runtime |
|---|---|---|
| Content | serializable POD | derived state (b2Body handle, GPU, caches, interpolation) |
| Entity refs | GUID only (`WorldGuidRegistry`) | may hold live handles |
| Raw pointers | forbidden | allowed (never serialized) |
| Snapshot / save | captured | skipped (tagged) |
| On PIE / load | transferred | **rebuilt by the owning world subsystem, on its first `Update`** (§3, WS7) |

- A component wanting both is **split in two** (`Rigidbody` authoring = mass/friction/type; runtime = `b2Body` handle).
- `ComponentRegistry` **v2** records the classification and per-type serialize functions — a rebuild, not a migration: the current one includes `WorldOld`, inherits the OOP component, and embeds the editor drawer under ifdef inside the engine. Its type-erased-entry concept is kept as reference; nothing else.
- **Registration point:** `RegisterModule()` (D9). Snapshot, PIE and save then work for game components with the editor knowing nothing about the types.
- **No OOP components in the registry.** `ComponentBase` stays a value type, no vtable. Per-entity behavior = authoring data `{behavior type + params}` + a world subsystem that instantiates and ticks the live objects. The old `IOpaaxComponent : ISerializable` is **not migrated**.

### D8 — The editor is a library; editor exes are thin
There is no "editor for the game". `OpaaxEditorLib` is generic; a game's editor executable is a ~10-line composition that registers the game's runtime module (D9) and its editor module (D10). The editor is generic; the game registers *into* it — never the reverse. `SandboxEditor.exe` is the M0–M2 dev host; `GameEditor.exe` is just one more instantiation — the proof that D8 holds.

### D9 — The GameModule is the unit of reuse
Game content (components, maps, world subsystems) lives in `Game/Module`, a static lib linked by **both** exes — not in the application subclass.

One entry point, `RegisterModule(ModuleRegistrar&)`, invoked through the `OnRegisterModules` seam. The registrar exposes the **engine** registries:

- `Components()` → `ComponentRegistry` v2, with D7 classification + serialize functions;
- `WorldSubsystems()` → the single `WorldSubsystemRegistry` (§3).

`Game/Module` never includes an editor header. Game-specific editor code lives in `Game/Editor`, compiled only into `GameEditor.exe`.

Door kept open, not opened: the module boundary — one `RegisterModule()` entry point — is exactly the seam a `Game.dll` hot-code-reload would require someday. Not built; not closed.

### D10 — Game editor extensions: registration, not knowledge
The extension surface is **defined by the editor, consumed by the game**. Symmetric with D9, one entry point per layer:

`IEditorModule::OnRegister(EditorExtensionRegistrar&)` — implemented in `Game/Editor`, invoked by `EditorService` **before the registry seals** (§2 ordering is normative).

| Registry | What the game plugs in | Lands in |
|---|---|---|
| `Drawers()` | custom Inspector UI per game component | M2 |
| `Panels()` | tool panels (wave designer, dialogue editor, …) — factory receives `EditorContext&`; the editor owns lifecycle, docking, layout persistence | M2 |
| `ResourceTypes()` | game-defined file types, keyed by **extension**: browser icon, label, double-click (old `IAssetTypeActions` concept, rewritten injected). Named for the live vocabulary — `CResource`/`ResourceManager` — since `Asset` now means the retired `Legacy/Assets` world. | M2 |
| `Menus()` | menu entries / toolbar commands ("Tools → Validate Level"). **LIVE M5 S4** — `MenuRegistry`, a flat `{Path, FMenuCommand}` list whose nesting is computed at draw time by splitting the path on `/`. The command takes `EditorContext&` (D3), which is why the M0 `[] {}` placeholder could not survive the route going real. The editor's own `File/*` entries register through this same route, so the whole menu bar is registry-driven and there is only one `File` menu. `EditorRoute` — the counts-only skeleton every route started as — is **deleted** with it. | M5 |
| `EditWorldSystems()` | **a route into the single `WorldSubsystemRegistry`** — editor-supplied candidates whose `ShouldCreate` gates on `Edit`. Read-only visualization (trigger zones, patrol paths, spawn points) via `DebugDraw`. Not a separate mechanism. **LIVE M4 S5**, and literally the game-side `WorldSubsystemRoute` reused, not an editor copy of it (ARCHITECTURE.md **MR4**) — `Menus()` is now the last counts-only `EditorRoute`, and it leaves with M5. | M4 |

```cpp
// Game/Editor/ — compiled ONLY into GameEditor.exe
class ShmupEditorModule final : public IEditorModule
{
    void OnRegister(EditorExtensionRegistrar& InR) override
    {
        InR.Drawers().Register<WaveSpawnerComponent, WaveSpawnerDrawer>();
        InR.Panels().Register("Wave Designer",
            [](EditorContext& InCtx) { return MakeUnique<WavePanel>(InCtx); });
        InR.ResourceTypes().Register(ResourceTypeDesc{
            .Extension  = OPAAX_ID(".wave"),
            .Label      = OPAAX_ID("Wave Definition"),
            .Icon       = OpaaxString("[W]"),
            .OnActivate = [](EditorContext& InCtx, const ResourceFile& InFile) { /* open the Wave Designer */ } });
        InR.EditWorldSystems().Register<TriggerZoneOverlaySystem>();
    }
};
```

Native editor features go through the same registries wherever possible — dogfooding keeps the surface honest.

`ResourceTypes()` is the one route that takes a **descriptor instead of type parameters** (M2d): `Drawers()` needs a template because `TComponent` is a type the editor cannot name, but a file type is a string key plus two labels and a closure — there is nothing to erase. The extension is normalized (lower-cased, leading dot) and interned at registration, so the browser's per-file lookup is an integer compare. The GUID-backed catalog (`.meta` sidecars, rename-safe references) belongs behind this same route when it lands — but it was **deliberately NOT built in M5** (user decision), because the reasoning that put it there does not hold. "Map files reference entities by Guid" is true and irrelevant: those are *entity* guids, they live in `EntityMeta`, and they already work. A *resource* guid would only matter once a component FIELD names a resource, and none does — `DummyComponent` is position/size/colour, `HealthComponent` is two ints, and textures are still deferred. Building it now would be an API whose only caller is its own test. **Trigger: the first component field that references a resource** (a sprite's texture).

Supporting decision — **`DebugDraw` belongs to the engine, not the editor**: immediate-mode world-space primitives (lines, boxes, circles). Serves editor overlays *and* dev builds of `Game.exe`. Small renderer addition, lands in M2.

Future direction, deliberately not now: per-type field meta-description generating serialize *and* a generic drawer from one declaration. That is a reflection system — a rabbit hole. Handwritten drawers first; the registry makes the upgrade transparent later.

---

## 5. Milestones

| # | Name | Content | Gate (done means) |
|---|---|---|---|
| **M0** | Shell | App seams (`OnProvideServices`, `OnRegisterModules`, `TickFrame`); **Sandbox split into `Module/` + `Runtime/`**; `ModuleRegistrar` skeleton (`Components()` / `WorldSubsystems()` routes accept & store); `OpaaxEditorLib` built as a lib + `SandboxEditor.exe` dev host; `EditorApplication` + `EditorService`; ImGui as **overlay** on the current engine render; input filter + `ResetState` contract; `IEditorModule` + extension registrar skeleton | Dockspace + input capture work; `Sandbox.exe` behavior identical after the split; runtime targets carry zero editor code |
| **M1** | Viewport | D2 output contract, present moves host-side, `ViewportPanel` rewritten with `EditorContext` injection | The game lives only inside the panel; panel size drives engine resolution |
| **M2** | Panels & extensions | Hierarchy, Inspector, ResourceBrowser; **`Drawers()` / `Panels()` / `ResourceTypes()` consumed** — native panels register through the same path as game panels; **engine `DebugDraw` API** | Click-select + live transform edit; a Sandbox-module custom drawer *and* custom panel appear with zero changes to `OpaaxEditorLib` |
| **M3** | Snapshot core | `MapSerializer`/`MapFactory` on the new `World`: registry ↔ `MapData`; **`ComponentRegistry` v2** live, fed by `RegisterModules()`; `MapId` + `EntityMeta::OwnerMap` | Capture → instantiate round-trip yields an equivalent world (GUIDs preserved), **including module components** |
| **M4** | World subsystems + PIE | **`WorldSubsystemMgr` + sealed `WorldSubsystemRegistry` (§3, all three locks)**; `EWorldMode` as immutable `CreateWorld` parameter; `CloneWorld` (capture + instantiate); Play/Pause/Step/Stop toolbar panel + reserved keys; `EditWorldSystems()` route live. *(Input route switching by world mode and `ResetState` on stop deferred to **M-Input** — `InputManager` is still an empty shell, so calling `ResetState()` would have no caller and nothing to reset.)* | Play runs the game in the viewport with module subsystems live, runtime state rebuilt on the first `Update`; Stop restores the exact edit state; overlays visible in Edit worlds only |
| **M5** ✅ | Map/Level IO | `MapJson`/`MapFile`/`MapResource` + `LevelFile`/`LevelResource`/`LevelLoader`; `WorldSpec.LevelPath` and `FinishStartup` opening it; **`Menus()` live and `EditorRoute` deleted**; `EditorMapDocument` with Save / Save As / Open and a **derived** dirty flag. *Scope calls: the level manifest stays DATA (no `LevelManager`, no streaming), no recent-files list, and the GUID resource catalog is deferred until a component field names a resource.* | **MET** — both hosts build their world from `Assets/Levels/Main.opaaxlevel`; `SpawnDemoWorld` is gone |

- M0 renders ImGui *over* the existing engine output on purpose — it validates ImGui, the CMake targets and the event chain in isolation. The renderer is only touched in M1.
- The snapshot core (M3) sits deliberately **before** PIE (M4): PIE consumes it. M3 was in-memory only; **M5 added the file layer** (`MapResource`/`LevelResource` and the json/disk stack beneath them). `LevelManager` and streaming are still **not built** — the manifest is parsed to plain data and `LevelLoader` opens its maps, which is the indirection streaming will sit on top of. Trigger for `LevelManager`: the first need to load or unload a map *while the world runs*.
- The registration seams and registrar exist from M0 (empty and accepting); the machinery that consumes `WorldSubsystemRegistry` lands in M4.

---

## 6. Salvage policy — old editor & registry code

**Reference, not migration.**

| Salvage as reference | Do not migrate |
|---|---|
| Panels (Hierarchy, Inspector, AssetBrowser, Viewport) — layout & behavior | Every static (registries, singletons) |
| Component drawers — per-type UI logic | `EditorSubsystem` (wired to `EngineSubsystemBaseOld` / `CoreEngineApp*`) |
| `IAssetTypeActions` — concept kept, rewritten injected behind `ResourceTypes()` | `IOpaaxComponent` (OOP component) |
| `ComponentRegistry` — type-erased entry concept only | its `WorldOld`/OOP/json/drawer coupling |
| `IEditorUIBackend` GL/VK — the RHI/ImGui bridge is sound | `OPAAX_WITH_EDITOR` scattering |
| `GetViewportImage` pattern | |

The ~10 `OPAAX_WITH_EDITOR` ifdefs inside engine files get a **dedicated cleanup session after M2** — most die naturally once editor metadata moves editor-side through D10 (the `ComponentRegistry` drawer coupling is the canonical example). Tracked as `// FIXME`, not done mid-migration.

---

## 7. Out of scope (explicitly)

- ImGui multi-viewport (OS-detachable panels).
- Undo/redo — post-M2; panels route edits through identifiable mutation points to keep the door open.
- **Interactive gizmos** (viewport handle dragging) — picking + input-routing extension. Read-only `DebugDraw` overlays first.
- **Game code hot-reload (`Game.dll`)** — seam preserved by D9, feature not built.
- **Reflection / field meta-description** — future upgrade behind `Drawers()`.
- **World subsystem dependency graph** — registration order is the contract (§3); no resolver at this scale.
- Gameplay input contexts / action maps — game layer.
- Asset hot-reload during PIE.

---

## 8. Risks & open points

- **RendererManager is trivial today.** M1 defines the first real output structure — keep it minimal: world pass → RenderTarget, host present. No render-graph ambition inside this plan.
- **GUID remap on instantiate.** Inter-entity references resolve through GUIDs at instantiate time; entt IDs are never assumed stable across worlds. The M3 round-trip gate is the test.
- **Event ordering.** The editor must see window events before they are enqueued to the engine bus (`OpaaxApplication::OnEvent` wiring, lands in M0).
- **Registration ordering & lifetime.** The §2 flow is normative and host-enforced: engine natives → game module → editor module → seal → first world. Extension objects are owned by `EditorService` registries (D3) and torn down before the services they reference.
- **`ShouldCreate` discipline.** Pure, cheap, no side effects — it runs at every world creation, including every PIE start.
- **Registry granularity creep.** Five editor registries *is* the surface. A new extension kind must justify itself against "can an existing registry + `EditorContext` already do it".
- **Migration overlap.** Old world (`CoreEngineApp`, `MyProject`) and new world (`OpaaxApplication`, `Sandbox`) coexist on the branch. This plan builds exclusively on the new world; nothing here may add a dependency on `*Old` code.
