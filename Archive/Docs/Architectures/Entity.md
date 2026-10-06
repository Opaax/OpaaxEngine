# Opaax Engine — Scene & Entity Layer Architecture

**Status:** Accepted — pending final sign-off before Milestone kickoff
**Date:** 2026-07-16
**Scope:** Entity/Component/System layer, event flow, lifecycle, UI boundary, global-state policy
**Audience:** Whole team. This is the reference document for the *Scene & Entity Layer* milestone. Read it before touching any code in `Engine/Source/Scene/`.

> NOTE: Doc is written in English per team standard (same rule as code comments). Discussion happened in French; decisions are recorded here as the single source of truth.

---

## 1. Context and Goals

The engine currently has:

- A Subsystem architecture (`ISubsystem`, `IEngineSubsystem`, `ISubsystemManager`) — Unreal-style, owned by `CoreEngineApp`.
- `entt` as vendor. No Entity/Scene layer exists yet. We start from a blank page.

Goals, in priority order (project standard):

1. **Long-term architecture** — must scale to a shippable 2D platformer with thousands of live entities (tiles, particles, projectiles).
2. **Correctness** — no dangling entities, no destruction-during-iteration, deterministic frame order.
3. **Performance** — hot paths are pure data iteration; virtual dispatch confined to cold paths.
4. **Production comfort** — gameplay developers coming from Unreal/Unity must be productive on day one without learning `entt` internals.

Non-goals for this milestone: screen-space UI framework, physics implementation, networking, editor. The design must not *block* them, but we do not build them now.

---

## 2. High-Level Design

```
CoreEngineApp
 ├─ Subsystems (renderer, input, audio, ...)        [existing]
 ├─ LayerStack
 │   ├─ GameLayer ── owns ──▶ Scene
 │   └─ UILayer (imgui dev HUD, later real HUD)
 │
 Scene
  ├─ entt::registry            (single source of truth for all world data)
  ├─ registry.ctx()            (per-scene singletons: CombatState, per-scene RNG, ...)
  ├─ EventQueue                (per-scene, drained every frame)
  ├─ GUID -> entt::entity map  (persistent identity resolution)
  ├─ System list               (explicit, ordered, hardcoded in ONE place)
  └─ ServiceContext            (injected refs: input, audio, time — owned by the app)
```

**Ownership is a tree.** Everything has exactly one owner. Dependencies are injected at construction — nothing reaches out to a global. See §9.

### Frame order (explicit, single file, readable in 10 seconds)

```
Input -> Scripts -> Physics -> Animation -> [game systems] -> Cleanup (deferred destroy) -> Render
```

Order is a hardcoded list in `Scene`, not a discovered graph. "System A depends on System B" is expressed as *a line above another line*. This is a feature: the frame is debuggable by reading one function.

---

## 3. Entity — a handle, not an object

| Rule | Rationale |
|------|-----------|
| `Entity` is `final` | No inheritance from Entity, ever. |
| Contents: `entt::entity` + `Scene*` (≤ 16 bytes) | Trivially copyable, passed by value, zero allocation. |
| **No virtual functions** | A vtable pointer breaks value semantics and requires stable addresses — that road leads to reinventing Unity's GameObject with all its cache problems. |
| Created **only** via `Scene::CreateEntity()` | Never through the raw registry. The Scene API is the single entry point; this is how base components are guaranteed. |

The truth lives in the registry. `Entity` is sugar over `registry` calls (`AddComponent<T>`, `GetComponent<T>`, `HasComponent<T>`).

### Base components (mandatory, added by `CreateEntity`)

| Component | Why non-negotiable |
|-----------|-------------------|
| `IDComponent` (GUID) | `entt::entity` is a *runtime* id: recycled, unstable across loads. Serialization, save games, and prefab cross-references need persistent identity. GUID→entity map lives in the Scene. |
| `TagComponent` (name) | Editor, logs, debug. Three lines of code, enormous value. |
| `TransformComponent` | ~40 bytes in 2D, needed by 99% of entities. Making it optional forces `HasComponent` checks in every system to save nothing. |

Pure-logic "entities" (game rules, level timer): if it exists once per scene and has no position, it is **not an entity** — it is a singleton in `registry.ctx()`.

Parent/child hierarchy: `RelationshipComponent` (future milestone), **never** inheritance.

---

## 4. Components and Systems

- **Components are data.** Plain structs. No logic, no inheritance. One exception: `NativeScriptComponent` (§5).
- **Systems act.** They iterate views and mutate components. Inheritance is acceptable here (`ISubsystem` already exists; one virtual call per system per frame is free).
- **Systems never talk to each other.** They leave data behind (events, state) that later systems read. No cross-system pointers, no callbacks between systems. Dependency = producer, data, consumer.
- A system needing an engine service (renderer, audio device) receives a reference at construction. The renderer itself stays **outside** the ECS — forcing it in would be dogma, not architecture.

---

## 5. Scripting — inheritance confined to one component

The Unreal-comfort layer. Inheritance lives in exactly one place and is opt-in per entity.

```
Registry
  └─ NativeScriptComponent ──ptr──▶ PlayerController : ScriptableEntity
                                        └─ m_Entity (handle, by value) ──▶ Registry
```

- `NativeScriptComponent` holds: an instance pointer (null until instantiated) + a factory function pointer. `Bind<T>()` stores a *recipe*, creates nothing — this is what makes scenes serializable.
- `ScriptableEntity` holds an `Entity` handle, injected by the Scene at instantiation. `GetComponent<T>()` delegates to the handle. The script stores **no gameplay state** — behavior only.
- The Scene is the glue: instantiates scripts (at BeginPlay, or at spawn during play), injects the handle, dispatches `OnCreate` / `OnUpdate` / `OnDestroy` / `OnEnable` / `OnDisable` / `OnTriggerEnter`.
- Neither `Entity` nor the ECS knows `ScriptableEntity` exists. **Litmus test:** delete the whole scripting system tomorrow — the ECS must not change by one line.

### Analogy for Unreal/Unity refugees

`ScriptableEntity` is a **MonoBehaviour**, or an ActorComponent-with-tick. It is **not** an Actor: an AActor *is* the entity and owns transform/identity/everything (which is why it weighs ~1 KB and spawning 5000 hurts). Here, the "Actor" is the Entity + its components in the registry — and *that* has no class.

### Team rules (the #1 risk of this pattern is recreating Unreal's hierarchy)

1. **Max two inheritance levels** below `ScriptableEntity`.
2. Any state read by a system, serialized, or shown in an editor goes in a **data component**. Scripts keep only transient behavior state (internal timers, FSM state).
3. A script getting big is split into multiple script components on the same entity — not deeper inheritance.
4. Expected production ratio: ~80% of logic in a handful of generic systems, ~20% in specific orchestration scripts (player, bosses, triggers). Tiles/particles/projectiles never get a script.

### Ownership decision

Script instance is owned by the component (destroyed with the entity). Migrate to a Scene-owned pool only if profiling justifies it.
`// NOTE:` this in the code when written.

---

## 6. Events — data in transit, not control flow

There **are** events. What is banned is callbacks wired *between systems*.

- **Per-scene `EventQueue`**, drained every frame. Events are pushed by producers (e.g. physics pushes `TriggerEnterEvent{trigger, other}` — physics does not know what a trigger *means*).
- **No subscription.** No listeners, no subscribe/unsubscribe, nothing to unhook when an entity dies. Systems read the queue and filter; the Scene dispatches entity-scoped events (trigger overlaps) to that entity's script if one exists. Entities themselves are passive data — they *receive* nothing.
- Any system may observe any event (achievements system sees all kills without the combat system knowing it exists). Readable-by-all is a feature.
- **Two consumption styles coexist**, chosen per case:
  - *ECS style:* a `CombatSystem` drains events, checks for `CombatArenaComponent`, reacts. For behavior repeated across many entities.
  - *Script style:* `ArenaTrigger : ScriptableEntity` gets `OnTriggerEnter`. For one-off orchestration. Twenty imperative lines, Unreal-style comfort.

### Cascade example ("player enters trigger, combat starts, walls spawn")

1. Frame N: physics detects overlap, pushes `TriggerEnterEvent`.
2. Same frame, later phase: `ArenaTrigger` script (or CombatSystem) receives it, writes `CombatState` singleton into `registry.ctx()`, spawns wall entities via the Scene.
3. Frame N+1: camera system *sees* `CombatState::Active` and locks; audio system sees the transition and starts combat music; world-UI shows health bars. **Nobody called anybody.** Combat "started" because world state changed and every system reads the world.

### The trade-off, stated honestly

Deferred events cost **one frame of latency** (16 ms @ 60 fps — imperceptible), and debugging changes nature: inspect queues and state instead of walking a call stack. In exchange: no destruction during iteration, no surprise execution order, no delegate dangling on a dead entity — the bug family that costs the most at the end of an Unreal production. For genuinely frame-critical cases (hitstop, parry), scripts *may* act immediately inside their callback — the Scripts phase has a fixed, safe slot in the frame precisely for this.

### The golden rule: events vs state

> **Events are for reactions. State is the truth.**

Events are disposable by nature. If an information must *not* be missable (boss dead, quest advanced, door open), it is **not an event** — it is persistent state (component or ctx singleton). A re-enabled entity does not replay missed events; it reads the world as it is (`CombatState` says combat is ongoing — it never needed the "combat started" event). If you catch yourself wanting event replay, that information should have been state.

This same rule is what will make save games and (potential) netcode tractable: everything that matters is serializable state; events are intra-frame transport only.

---

## 7. Lifecycle, deferred mutation, disable, culling

### Lifecycle semantics

- `BeginPlay` ≠ constructed. It means "the scene enters play mode and I am in it". Driven by `Scene::OnBeginPlay / OnUpdate / OnEndPlay`.
- Pure construction/destruction hooks: `entt` signals (`on_construct` / `on_destroy`). No inheritance needed.

### Deferred mutation (mandatory)

Never destroy immediately. Tag `DestroyPending` (or push to a command buffer); a Cleanup system at end of frame executes destruction. Same for spawning during iteration. This structurally removes the "pending kill / dangling pointer" bug class.

### Disable is a tag; communication is free

- `DisabledTag` = empty component. Systems exclude it in their views: `view<Transform, Velocity>(exclude<DisabledTag>)`. Disabled entity is simply not iterated. No `enabled` flag checked in every component, no propagation, no message, no forgotten `SetActorTickEnabled` cascade.
- Exactly **two active reactions** to the tag appearing/disappearing (via entt signals):
  1. Scene dispatches `OnDisable` / `OnEnable` to the entity's script, if any (free a timer, stop a sound).
  2. Physics system sleeps/wakes the corresponding body.

### Culling is NOT disable

Render culling is an internal renderer filter, recomputed every frame, notifying **no one**. An off-screen enemy keeps patrolling; an off-screen projectile keeps flying. Coupling logic to camera frustum (`OnBecameInvisible`-style) makes gameplay depend on window size — banned by design. Real deactivation (streaming, distance sleep, zone pause) is the `DisabledTag`, an explicit gameplay/streaming decision.

---

## 8. UI boundary

Two problems that share three letters and nothing else:

| | World UI | Screen HUD |
|---|---|---|
| Examples | Health bar over enemy, floating damage, NPC speech bubble | Score, lives, menus, pause |
| Lives in | **ECS entities.** World space, transform, `RelationshipComponent` parenting, same render pipeline. | **Separate layer, outside ECS.** `UILayer` above `GameLayer` in the LayerStack. |
| Why | Zero reason to special-case them. | Screen coordinates, anchoring/resize layout, deterministic draw order, input focus & consumption, survives scene changes. Forcing it into entities = the Unity Canvas trap. |

- Input goes **down** the layer stack (each layer may consume); rendering goes **up**.
- HUD **reads** game state through components (`HealthComponent`) — never owns or writes it.
- Short term: dev HUD is **imgui** (already vendored). The real UI framework (anchors, font atlas, nine-slice) is its own milestone, opened only when the game needs it. Designing it now is speculative architecture.

---

## 9. Global-state policy: no statics, no singletons

**Rule (enters CODE STANDARDS): non-const `static` is forbidden in `Engine/Source`, except the logger. Any exception goes through review.** Enforceable by grep.

Why: a singleton is an invisible dependency — anyone can touch anything, discovered only by reading every .cpp. Without statics: the dependency graph *is* the constructor signatures; two simultaneous scenes become possible (editor + PIE later); unit tests need no global setup; init/shutdown order is explicit (static-initialization-order fiasco cannot exist); clean reload.

Mechanics:

- **Ownership tree, everything descends.** App → Subsystems + LayerStack → GameLayer → Scene → registry/events/ctx. Injected at construction. Nobody "goes and gets" — everybody *receives*.
- `registry.ctx()` is **not** a banned singleton: it is per-scene, owned by the registry, dies with it. "One instance per scene" ≠ "globally reachable via static". Two scenes = two `CombatState`.
- **ServiceContext:** the Scene receives at construction a small set of service references (input, audio, time — app-owned) and exposes them to scripts through the handle (`GetInput().IsKeyDown(...)`). Same comfort as a static, traceable ownership chain.
  **Guard:** it holds the 4–5 base gameplay services; every addition is justified in review. A grow-forever ServiceContext is a Service Locator, i.e. a singleton with an extra step.
- **Per-scene RNG** (determinism, replays), time, allocators, config: all through the tree.

Two honest, documented exceptions:

1. **Logging** (spdlog macros, `OPAAX_CORE_*`): write-only, no effect on logic — the singleton danger (state mutated by anyone) does not apply. Injecting a logger into every constructor is purity that costs more than it buys.
2. **Asserts / crash handler:** same reasoning.

Known friction: C callbacks (GLFW) take function pointers, not capturing lambdas. Standard no-static solution: `glfwSetWindowUserPointer` carries the context. Handled at the windowing layer — gameplay never sees it.

---

## 10. Decisions summary (the tablet of law)

| # | Decision |
|---|----------|
| D1 | `Entity` = final handle (id + scene ptr), value semantics, **zero virtuals**. Created only via `Scene::CreateEntity`. |
| D2 | Mandatory base components: `IDComponent` (GUID), `TagComponent`, `TransformComponent`. |
| D3 | Components = data structs, no inheritance. Single exception: `NativeScriptComponent`. |
| D4 | Inheritance allowed in: systems, and script classes under `ScriptableEntity` (max 2 levels). |
| D5 | Scripts hold behavior only; shared/serialized state lives in data components. |
| D6 | System execution order = explicit hardcoded list in `Scene`, one file. |
| D7 | Systems communicate through data (events + ctx state), never through direct calls. |
| D8 | Per-scene `EventQueue`, no subscription model, drained each frame. Events disposable; truth is state. |
| D9 | All entity destruction/spawn during play is deferred to end-of-frame Cleanup. |
| D10 | Disable = `DisabledTag` + view exclusion. Signals notify scripts + physics only. |
| D11 | Render culling notifies no one; never coupled to logic. |
| D12 | World UI = entities. Screen HUD = separate `UILayer`, reads state, never owns it. Dev HUD = imgui. |
| D13 | No non-const statics/singletons in engine code. Exceptions: logger, asserts. `registry.ctx()` and ServiceContext are per-scene/injected, not globals. |
| D14 | Script instances owned by their component (pool migration only if profiling demands). |
| D15 | Hierarchy via `RelationshipComponent` (future), never inheritance. |

## 11. Consequences

**Easier:** serialization & save games (state-is-truth), editor + runtime side by side, unit testing, onboarding Unreal/Unity devs (ScriptableEntity), performance at entity counts a platformer needs.
**Harder:** one-frame event latency (imperceptible), debugging by state inspection instead of call stacks, constructor plumbing discipline.
**Revisit later:** script instance pooling (profiling-gated), pair-wise interactions inside physics (physics milestone), real UI framework (own milestone), `RelationshipComponent` design.

## 12. Milestone — Scene & Entity Layer (implementation order)

1. [ ] Ownership tree + LayerStack skeleton (`GameLayer` owning `Scene`); ServiceContext plumbing.
2. [ ] `Scene` + frame order function; `Entity` handle; base components; GUID map.
3. [ ] `EventQueue` + deferred destroy (Cleanup system) — the skeleton everything inherits from.
4. [ ] `NativeScriptComponent` + `ScriptableEntity` + Scene dispatch (Create/Update/Destroy/Enable/Disable).
5. [ ] `DisabledTag` + signal hooks; per-scene RNG in ctx.
6. [ ] GLFW user-pointer context routing (windowing side).
7. [ ] Validation sample in the game layer: a trigger + script + spawned entities exercising the full event path.

Open question to settle at step 4 (flagged during design): script instantiation timing — at spawn vs at BeginPlay — must support spawning during play. Current answer: instantiate at BeginPlay for pre-placed entities, at spawn for runtime-created ones. Confirm at implementation.
