# Opaax Engine — Event System Architecture

**Status:** Accepted — implementation milestones defined below
**Scope:** `Engine/Source/Core/Events/`
**Standard:** C++20, no exceptions, no RTTI, no implicit heap allocation in hot paths

---

## 1. Context

Opaax currently has no event layer of its own. GLFW callbacks reach `Window`
implementations directly, and there is no sanctioned way for systems to talk
to each other without holding raw pointers. Input, UI (imgui), and gameplay
systems are about to be built on top of this — the transport layer must exist
first.

We adopt a **three-tier architecture**. Each tier is a different *transport*
with different guarantees. They share the same payload definitions and never
overlap in responsibility.

| Tier | Name                | Coupling            | Dispatch            | Consumable |
|------|---------------------|---------------------|---------------------|------------|
| 1    | Dispatched Events   | Source → LayerStack | Immediate, ordered  | Yes        |
| 2    | Delegates           | Object → Object     | Immediate, in-order | No         |
| 3    | Event Bus           | None (pub/sub)      | Immediate OR queued | No         |

**Selection rule (memorize this):**
- Propagation order / consumption matters → **Tier 1** (input, window, UI).
- You already hold a reference to the emitter → **Tier 2** (delegate member).
- Sender and receiver must be strangers → **Tier 3** (bus).

---

## 2. File Layout

```
Engine/Source/Core/Events/
├── EventTypes.h        // Shared payload structs + EventType/Category enums
├── Event.h             // Tier 1: Event base + EventDispatcher
├── WindowEvents.h      // Tier 1: Window/App event classes
├── InputEvents.h       // Tier 1: Key/Mouse event classes
├── Delegate.h          // Tier 2: TOpaaxDelegate / TOpaaxMulticastDelegate
├── DelegateHandle.h    // Tier 2: Handle + lifetime management
├── EventBus.h/.cpp     // Tier 3: Bus subsystem (immediate + queued)
```

// NOTE: One payload, one definition. `WindowResizeEvent` data (width, height)
// lives once in EventTypes.h. Tier 1 wraps it in an Event class; Tier 2
// and Tier 3 pass the plain struct. Never duplicate payload shapes per tier.

---

## 3. Tier 1 — Dispatched Events (blocking, consumable)

### Purpose
Deliver OS/window/input events synchronously down the layer stack, top-first
(UI before gameplay), with the ability for any receiver to consume the event
and stop propagation.

### Pseudo-doc

```
enum class EEventType : Uint8
    None, WindowClose, WindowResize, WindowFocus, WindowLostFocus,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled

enum EEventCategory : Uint8 (bitmask)
    None        = 0
    Application = BIT(0)
    Input       = BIT(1)
    Keyboard    = BIT(2)
    Mouse       = BIT(3)
    MouseButton = BIT(4)

class Event (abstract)
    // -- Interface --
    GetEventType()      -> EEventType      // static + virtual pair (no RTTI)
    GetCategoryFlags()  -> Uint8
    IsInCategory(cat)   -> bool
    ToString()          -> String          // debug builds only
    // -- State --
    bHandled : bool     // set by a receiver to stop propagation

class EventDispatcher
    // Constructed on the stack around a live event reference.
    ctor(Event& InEvent)
    Dispatch<T>(Func<bool(T&)> InHandler) -> bool
        // Compares T::GetStaticType() against the event's runtime type.
        // On match: invokes handler, ORs the returned bool into bHandled.
        // No dynamic_cast, no RTTI — type check is an enum compare.
```

### Flow

```
GLFW callback (WindowsWindow)
   └─> constructs concrete event ON THE STACK
       └─> Window's EventCallback (bound by App at init)
           └─> App::OnEvent(OpaaxEvent&)
               └─> LayerStack reverse iteration (overlays/UI first)
                   └─> Layer::OnEvent(Event)  — uses EventDispatcher
                   └─> stop when Event.bHandled == true
```

### Rules
- Events are stack-allocated, live only inside the callback. **Never store
  a Tier 1 event pointer.** If a system needs the data later, copy the
  payload struct and republish on the bus (Tier 3).
- PERF: zero heap allocation on this path. It runs per keystroke/mouse move.
- Gameplay systems never subscribe here. This tier ends at the layer stack.

---

## 4. Tier 2 — Delegates (Unreal-style, instance-bound)

### Purpose
Point-to-point notification between objects that already know each other.
A delegate is a *member* of the emitting class, not a system.

### Pseudo-doc

```
class DelegateHandle
    // Opaque 64-bit id. Returned by every bind. Required by every unbind.
    IsValid() -> bool
    Reset()

template<typename... Args>
class TDelegate            // single-cast
    Bind(Func<void(Args...)>)          -> void
    BindMember(T* Obj, MemberFn)       -> void
    Unbind()                           -> void
    IsBound()                          -> bool
    Execute(Args...)                   -> void   // asserts if unbound
    ExecuteIfBound(Args...)            -> void

template<typename... Args>
class TMulticastDelegate   // one-to-many
    Add(Func<void(Args...)>)           -> OpaaxDelegateHandle
    AddMember(T* Obj, MemberFn)        -> OpaaxDelegateHandle
    Remove(OpaaxDelegateHandle)        -> bool
    RemoveAll(void* Obj)               -> void   // bulk-unbind by owner
    Broadcast(Args...)                 -> void   // invokes in bind order
    Clear()

// Declaration macros (Unreal-familiar surface):
DECLARE_DELEGATE(FOnClosed)
DECLARE_DELEGATE_OneParam(FOnHealthChanged, Int32)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnResized, Uint32, Uint32)
```

### Usage pattern

```
class Window
    FOnResized OnResized;      // public multicast member

Renderer::Init(Window& W)
    m_ResizeHandle = W.OnResized.AddMember(this, &Renderer::OnWindowResized);

Renderer::Shutdown()
    m_Window->OnResized.Remove(m_ResizeHandle);   // MANDATORY
```

### Rules — lifetime (this is where delegate systems die)
- **Every `Add` must be paired with a `Remove`.** A listener that is
  destroyed while bound = dangling callback = crash on next Broadcast.
- Listeners store their `DelegateHandle` as a member and remove in
  their destructor / Shutdown. No exceptions to this rule.
- `Broadcast` must be re-entrancy safe: a callback may Remove itself during
  broadcast. Implementation iterates over a stable snapshot or uses
  deferred-removal marking. // FIXME: decide at implementation time,
  snapshot copy is simpler, deferred marking is allocation-free.
- No "handled" concept. Every bound listener is invoked, in bind order.
- Delegates never cross system boundaries where the two sides shouldn't
  know each other — that is the bus's job.

### Canonical uses in Opaax
- `Window.OnResized`, `Window.OnClosed` → renderer / engine react.
- Engine lifecycle: `CoreEngineApp.OnPreShutdown` → subsystems flush.
- Future UI widgets: `Button.OnClicked`.

---

## 5. Tier 3 — Event Bus (decoupled pub/sub)

### Purpose
Many-to-many communication between systems that must not reference each
other. Implemented as an `IEngineSubsystem` owned by `Engine`.

### Pseudo-doc

```
class EventBusSubsystem : IEngineSubsystem
    // -- Subscription --
    Subscribe<TEvent>(Func<void(const TEvent&)>) -> DelegateHandle
    Subscribe<TEvent>(T* Obj, MemberFn)          -> DelegateHandle
    Unsubscribe<TEvent>(OpaaxDelegateHandle)     -> bool
    UnsubscribeAll(void* Obj)                    -> void

    // -- Publishing --
    Publish<TEvent>(const TEvent&)   -> void
        // IMMEDIATE: invokes all subscribers now, on the calling thread.
        // Use for rare, latency-critical signals only.

    Enqueue<TEvent>(TEvent&&)        -> void
        // QUEUED: copies the payload into the frame queue.
        // Delivered at the flush point. This is the DEFAULT choice.

    // -- Frame integration --
    Flush()                          -> void
        // Called ONCE per frame by Engine, at a fixed point
        // (after input dispatch, before game update).
        // Drains the queue in FIFO order. Events enqueued DURING flush
        // land in the next frame's queue (double-buffered) — this kills
        // infinite publish loops by construction.
```

### Type identity without RTTI

```
// NOTE: TEvent identity = monotonic Uint32 assigned on first use via a
// templated static local counter (the standard "family id" trick, same
// approach entt uses internally). No RTTI, no strings, no hashing.
// Payloads must be trivially copyable structs — enforced with
// static_assert(std::is_trivially_copyable_v<TEvent>).
```

### Rules
- **Queued is the default.** Immediate publish is the exception and must be
  justified in a // NOTE:.
- Payload structs are plain data. No pointers to frame-transient memory
  inside a queued event — it outlives the publishing scope.
- No ordering guarantees *between subscribers* of the same event. If order
  matters between two systems, you picked the wrong tier — use a delegate.
- PERF: the queue uses a per-frame linear allocator (bump + reset at Flush).
  No per-event heap allocation. // TODO: linear allocator is Milestone E4.
- Not thread-safe in v1. // FIXME: single-threaded contract — revisit when
  job system lands. Assert on cross-thread Enqueue in debug builds.

### Canonical uses in Opaax
- `LevelLoadedEvent` → audio, streaming, editor tooling.
- `EntityDestroyedEvent` (bridged from entt signals) → any interested system.
- Editor/runtime boundary: engine publishes, tools observe.

---

## 6. Frame Timing (where each tier runs)

```
┌─ Frame N ──────────────────────────────────────────────┐
│ Window::PollEvents()                                   │
│   └─ Tier 1 events fire (blocking, through LayerStack) │
│       └─ handlers may Enqueue on Tier 3                │
│ EventBus::Flush()          ← single fixed flush point  │
│   └─ Tier 3 queued events delivered                    │
│ Game / Systems Update                                  │
│   └─ Tier 2 Broadcasts fire inline as state changes    │
│   └─ systems may Enqueue (lands in Frame N+1)          │
│ Render                                                 │
│ SwapBuffers                                            │
└────────────────────────────────────────────────────────┘
```

// NOTE: Exactly ONE flush point. Adding a second one "because system X
// needs the event earlier" is how event systems become undebuggable.
// If a system needs it earlier, it uses immediate Publish with a NOTE.

---

## 7. Trade-offs Accepted

- **Three mechanisms, not one.** More surface than a single mega-bus, but
  each tier is small and single-purpose. A unified system would force the
  "handled" concept, lifetime handles, and queuing into every call site.
- **Trivially-copyable bus payloads.** Excludes String-carrying events in
  v1. Acceptable: engine events are IDs and numbers. Revisit if needed.
- **Single-threaded bus in v1.** Correct for current engine; the flush-point
  contract is designed so an MPSC queue can slot in later without API change.

---

## 8. Milestones

- **E1 — Tier 2 Delegates.** Foundation; Tier 1 callback binding and Tier 3
  subscriber lists are built on it. Deliverable: delegate templates, handle,
  macros, re-entrancy-safe Broadcast, unit-testable in isolation.
- **E2 — Tier 1 Dispatched Events.** Event base, dispatcher, window/input
  event classes, `App::OnEvent` + LayerStack propagation. GLFW
  callbacks rewired through it.
- **E3 — Tier 3 Event Bus.** Subsystem, family-id registry, immediate +
  queued paths, double-buffered queue, Flush integrated into the frame loop.
- **E4 — Hardening.** Linear allocator for the bus queue, debug event
  tracing (log every dispatched/flushed event under a verbosity flag),
  entt signal bridge.

Each milestone starts with a fresh codebase analysis per team process.
