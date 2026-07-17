# Renderer Module — Architecture

**Status:** Accepted — M-RENDER-01
**Scope:** Design document with pseudocode. No compilable code here; implementation lands per milestone.
**Audience:** Engine team. Assumes C++20, no exceptions, no RTTI, data-oriented bias.

---

## 1. Purpose

A 2D renderer built as a **standalone module**. It must run inside this engine today and inside
any other host tomorrow. The host could be our engine loop, an editor, a headless tool generating
sprite atlases previews — the renderer does not know and does not care.

The single rule that makes this possible:

> **Dependencies point inward only.** The host depends on the renderer. The renderer depends on
> nothing from the host — no subsystem interfaces, no window class, no app class, no ECS.

If a type name from the host ever appears inside `Renderer/`, portability is dead. That is the
line reviews enforce first.

## 2. Design goals and constraints

The module is written against the team's existing standards: C++20, `noexcept` everywhere, no
RTTI, no implicit heap allocation in hot paths, PascalCase / m_PascalCase / UPPER_SNAKE_CASE.
On top of that, four module-specific goals, in priority order:

1. **Backend-portable.** OpenGL 4.6 core now; the RHI seam is real from day one so a Vulkan
   backend is an addition, not a rewrite.
2. **Host-portable.** The module compiles as its own static library with only a math dependency
   and the backend's loader (Glad). Everything else is injected.
3. **Data-oriented.** GPU resources are opaque handles into pools, not virtual objects on the
   heap. Draw submission is packed data, not virtual calls per sprite.
4. **Deterministic lifetime.** Explicit `Init` / `Shutdown`, explicit resource destruction,
   generation-checked handles so use-after-free is caught in dev builds instead of crashing.

What the module may depend on: `glm` (or a thin math alias header), the graphics loader owned by
each backend, and the C++ standard library. Nothing else.

## 3. Layer overview

```
                    HOST SIDE (any engine)                 MODULE SIDE (Opaax.Renderer)
  ┌────────────────────────────────────────┐   ┌──────────────────────────────────────────┐
  │  Host loop / subsystem adapter         │   │  RenderSystem                            │
  │  - owns a RenderSystem instance        ├──►│  - lifecycle, frame orchestration        │
  │  - creates the OS window/surface       │   │  - owns Renderer2D + IRHIDevice          │
  │  - builds RenderView from its world    │   ├──────────────────────────────────────────┤
  │                                        │   │  Renderer2D                              │
  │  Host world (ECS, gameplay)            │   │  - BeginScene(RenderView) / Draw / End   │
  │  - camera components, transforms       │   │  - quad batching, texture slots          │
  │  - CameraSystem builds RenderView ─────┼──►├──────────────────────────────────────────┤
  └────────────────────────────────────────┘   │  IRHIDevice (abstract)                   │
                                               │  - handles, descriptors, submission      │
              contracts crossing the boundary: ├──────────────────────────────────────────┤
              - RenderSystemDesc (init)        │  OpenGLRHIDevice (backend)               │
              - RenderView (per frame)         │  - GL context, Glad, state cache, swap   │
              - RenderStats (out)              └──────────────────────────────────────────┘
```

Three things cross the boundary, and only three: an init descriptor, a per-frame view snapshot,
and stats going back out. All of them are plain data.

## 4. The boundary contracts

### 4.1 RenderSystemDesc — initialization

The host hands the module everything it needs at startup as one descriptor. The surface is an
opaque native handle; the module never includes GLFW, SDL, or Win32 headers at this layer.

```
// NOTE: Pseudocode. Types simplified; error handling shown where it matters.

enum ERHIBackend { OpenGL, /* Vulkan later */ }

struct SurfaceDesc
{
    void*  NativeHandle    // HWND, GLFWwindow*, whatever the host has
    u32    Width, Height
    bool   VSync
}

struct RenderServices
{
    // NOTE: Injected callbacks — the module never links against the host's logger.
    //       Defaults are provided (stderr, malloc) so the module runs with zero setup.
    LogFn     Log       = DefaultLog
    AllocFn   Alloc     = DefaultAlloc
    FreeFn    Free      = DefaultFree
}

struct RenderSystemDesc
{
    ERHIBackend     Backend
    SurfaceDesc     Surface
    RenderServices  Services
    RenderLimits    Limits      // max quads per batch, max texture slots, pool sizes
}
```

### 4.2 RenderView — the camera contract

The module has **no camera class**. It consumes a per-frame snapshot:

```
struct RenderView
{
    mat4     ViewMatrix        // inverse of the camera world transform
    mat4     ProjectionMatrix  // orthographic for the platformer; the module doesn't care
    Viewport Viewport          // x, y, w, h in pixels
}
```

Why this shape and not a camera object:

- **Ownership.** Camera state (position, zoom, follow target, shake) is world data. It lives in
  the host's ECS as a component; gameplay systems mutate it. The renderer receiving a snapshot
  means camera logic can be arbitrarily game-specific without the module knowing.
- **Multiplicity for free.** Split-screen, editor viewport, minimap render target — each is just
  another `RenderView` and another `BeginScene`. There is no "the" camera to fight over.
- **Portability.** Another engine with a different scene model produces the same two matrices.
  The contract is the math, not the ownership.

Long term the host owns a `CameraSystem` that, once per frame, selects the primary camera entity,
composes the matrices, and hands the snapshot over:

```
// HOST-SIDE pseudocode — lives in the world layer, not the module.
CameraSystem::BuildView(world, framebufferSize) -> RenderView
{
    entity  = world.QueryPrimaryCamera()          // CameraComponent + TransformComponent
    view    = Inverse(entity.Transform.Matrix())
    proj    = Orthographic(entity.Camera.Size, framebufferSize.Aspect(),
                           entity.Camera.Near, entity.Camera.Far)
    return RenderView{ view, proj, FullViewport(framebufferSize) }
}
```

An editor free-fly camera is *not* an entity — the editor layer builds its own `RenderView`
directly. Same struct, different producer. That is the whole point of the POD boundary.

// NOTE: The module ships an `Orthographic()` math helper because every host needs it, and math
// has no ownership problems. It ships zero camera *behavior* (follow, smoothing, shake) —
// behavior is game logic.

## 5. RenderSystem — the portable orchestrator

The top-level object a host owns. One instance per surface for now.

```
class RenderSystem
{
    Init(RenderSystemDesc desc) -> Result
    {
        m_Device   = RHI::CreateDevice(desc.Backend, desc.Surface, desc.Services)
        if (!m_Device) return Result::BackendInitFailed

        m_Renderer2D.Init(m_Device, desc.Limits)
        return Result::Ok
    }

    Shutdown()                      // reverse order, explicit, idempotent
    Resize(u32 w, u32 h)            // host forwards surface resize events here

    BeginFrame()                    // device frame setup, stats reset
    EndFrame()                      // flush pending batches, Present()

    GetRenderer2D() -> Renderer2D&
    GetStats()      -> RenderStats  // draw calls, quads, batch flushes — cheap, always on
}
```

Lifecycle contract the host must respect, in order:

```
1. host asks module for surface preferences        (see 6.3 — before window creation!)
2. host creates its OS window/surface
3. RenderSystem::Init(desc)
4. per frame:  BeginFrame → N × [ BeginScene(view) → Draw* → EndScene ] → EndFrame
5. RenderSystem::Shutdown()
6. host destroys the window
```

// FIXME: Single-threaded contract for now — all calls from one thread, the one that owns the
// GL context. The API is shaped so a command-buffer split (record on game thread, submit on
// render thread) can be added behind it later. Do not add threading before profiling demands it.

## 6. RHI — the backend seam

### 6.1 Handles, not objects

```
// NOTE: 32-bit handle = 20-bit pool index + 12-bit generation. Trivially copyable, fits in
// registers, serializable. The generation counter catches use-after-destroy in dev builds:
// a stale handle's generation no longer matches the pool slot -> assert, not a driver crash.

struct BufferHandle   { u32 Value }   // and TextureHandle, ShaderHandle,
struct TextureHandle  { u32 Value }   // PipelineHandle, FramebufferHandle
...
INVALID_HANDLE = 0
```

The rejected alternative — virtual resource classes (`class Texture2D { virtual Bind() ... }`)
— is faster to write and is what most tutorial engines do. It costs a heap allocation and a
vtable per resource, encourages scattered `Bind()` calls that make state impossible to reason
about, and maps badly to Vulkan where "bind" isn't a thing. Handles cost us pool boilerplate
once and pay for themselves at the first backend port. Decision is final unless profiling of
*developer time* proves otherwise.

### 6.2 Creation descriptors and the device interface

```
struct BufferDesc   { EBufferUsage Usage; u32 SizeBytes; bool Dynamic }
struct TextureDesc  { u32 Width, Height; EPixelFormat Format; EFilter Filter; EWrap Wrap }
struct ShaderDesc   { StringView VertexSrc; StringView FragmentSrc }   // GLSL now; SPIR-V later
struct PipelineDesc { ShaderHandle Shader; VertexLayout Layout; BlendState Blend; ... }

interface IRHIDevice
{
    // --- resources: plain data in, handle out ---
    CreateBuffer(BufferDesc)        -> BufferHandle
    CreateTexture(TextureDesc)      -> TextureHandle
    CreateShader(ShaderDesc)        -> ShaderHandle
    CreatePipeline(PipelineDesc)    -> PipelineHandle
    Destroy(AnyHandle)

    UpdateBuffer(BufferHandle, offset, size, data)   // PERF: hot path for the quad batcher

    // --- frame ---
    BeginFrame()
    SetViewport(Viewport)
    Clear(ClearDesc)
    Submit(DrawCommand)             // pipeline + bindings + vertex/index range, packed POD
    Present()                       // buffer swap lives HERE, not in any window class
    Resize(u32 w, u32 h)

    GetCapabilities() -> RHICapabilities   // max texture slots, max texture size, ...
}

RHI::CreateDevice(backend, surface, services) -> IRHIDevice*
{
    switch (backend)
    {
        case OpenGL: return NewOpenGLDevice(surface, services)
        // NOTE: Vulkan slots in here. Nothing above this line changes.
    }
}
```

// NOTE: The device is stateless from the caller's point of view. Callers describe complete
// draws; the backend translates to GL binds internally and caches redundant state changes.
// Nobody outside the backend ever "binds" anything. This is the discipline that makes the
// Vulkan port mechanical instead of archaeological.

### 6.3 OpenGL backend specifics

The backend owns the GL context — creation, currency, destruction, and the swap. This inverts
the common (wrong) arrangement where the window class creates the context.

```
OpenGLRHIDevice::Init(surface, services)
{
    MakeContextCurrent(surface.NativeHandle)
    LoadGL(Glad)
    if (dev build) EnableDebugCallback(-> services.Log)   // KHR_debug, sync mode
    CreateStateCache()
}
```

**Surface preferences — the one place the module must speak before the window exists.**
A GL context's version/profile is fixed at window creation time on most platforms. The host
cannot create its window blind. So the module exposes a static query:

```
RHI::GetSurfacePreferences(ERHIBackend) -> SurfacePreferences
    // OpenGL: { ContextMajor:4, ContextMinor:6, CoreProfile:true, DebugContext:isDevBuild }
    // Vulkan: { NoClientAPI:true }
```

The host calls this, applies the hints with *its* windowing library (GLFW here, SDL elsewhere),
creates the window, then passes the handle in. The module stays windowing-agnostic; the host
stays graphics-agnostic. // NOTE: this query is the entire reason init step 1 exists in §5.

## 7. Renderer2D — the high-level API

What game code (through the host) actually touches. Immediate-mode API, retained batching inside.

```
class Renderer2D
{
    BeginScene(RenderView view)
        // uploads view-projection to the per-frame uniform buffer, resets the batch

    DrawQuad(vec2 pos, vec2 size, vec4 color)
    DrawSprite(vec2 pos, vec2 size, TextureHandle tex, UVRect uv, vec4 tint)
    DrawSpriteRotated(...)

    EndScene()   // flushes the final partial batch
}
```

Batching internals, because they define the performance envelope:

```
// PERF: One dynamic vertex buffer, MAX_QUADS_PER_BATCH * 4 vertices, written linearly on the
// CPU each frame. One shared static index buffer (pattern 0,1,2,2,3,0 repeated) built once.
// Texture-slot array sized from RHICapabilities (16..32). Flush when: quad budget hit, texture
// slots exhausted, or EndScene. Target: an entire platformer layer = 1-3 draw calls.

DrawSprite(...)
{
    if (m_QuadCount == MAX_QUADS || !TryAssignTextureSlot(tex))
        Flush()                       // UpdateBuffer + Submit, then reset

    WriteFourVertices(m_VertexCursor, pos, size, uv, tint, slotIndex)
    m_QuadCount++
}
```

`RenderStats` (draw calls, quads, flush reasons) is filled every frame from day one. The imgui
overlay on the host side reads it. You cannot manage what you cannot see, and retrofitting
counters after a perf problem appears is how studios lose weeks.

## 8. Frame lifecycle — end to end

```
// HOST loop (any engine):
world.Update(dt)                                  // gameplay moves the camera entity, etc.
view = cameraSystem.BuildView(world, fbSize)      // host-side, §4.2

renderSystem.BeginFrame()
r2d = renderSystem.GetRenderer2D()

r2d.BeginScene(view)
world.SubmitVisibleSprites(r2d)                   // host iterates its ECS, calls DrawSprite
r2d.EndScene()

// optional extra views — editor viewport, minimap — are just more Begin/End pairs
renderSystem.EndFrame()                           // flush + Present (swap)

window.PollEvents()                               // host concern, after present
```

Note what is *absent*: the module never polls events, never swaps "the window", never reads
input, never queries the ECS. Each of those absences is a portability guarantee.

## 9. Host integration — two examples

**This engine.** A thin subsystem adapter on the host side (≈40 lines, all glue):

```
class RendererSubsystem : IEngineSubsystem      // HOST code — module knows nothing of this
{
    Startup()  { prefs = RHI::GetSurfacePreferences(OpenGL)   // host applied these
                 m_RenderSystem.Init(BuildDescFromHost()) }    // before window creation
    Render(alpha) { /* the frame lifecycle from §8 */ }
    Shutdown() { m_RenderSystem.Shutdown() }
}
```

// FIXME: Current host debt blocking this: WindowsWindow calls glfwMakeContextCurrent and owns
// SwapBuffers. Both must be deleted from the window when the module lands — context and swap
// move behind IRHIDevice (§6.3). Window shrinks to: surface creation + events + native handle.
// Also: host init order must become  query prefs → create window → RenderSystem::Init.

**Another engine.** No subsystems, no adapter class needed — an SDL host is a dozen lines:

```
prefs = RHI::GetSurfacePreferences(OpenGL)
window = SDL_CreateWindow(... apply prefs ...)
rs.Init({ OpenGL, { NativeHandleOf(window), w, h, vsync }, { MyLog }, DefaultLimits })
loop { rs.BeginFrame(); r2d.BeginScene(myView); ...; r2d.EndScene(); rs.EndFrame() }
```

If that second example ever stops being a dozen lines, the module has grown a host dependency
and the design has regressed. Keep this snippet as the portability litmus test.

## 10. Error handling, lifetime, threading — the rules

**Errors.** No exceptions. Fallible operations return `Result` enums; resource creation returns
`INVALID_HANDLE` on failure and logs through the injected `LogFn`. Dev builds assert on invalid
handle use via the generation check; release builds skip the draw and count it in stats.

**Lifetime.** Explicit `Destroy(handle)` — no ref counting, no RAII wrappers around GPU objects
inside the module. The host layer may build RAII sugar on top if it wants; the module's job is
to be predictable, not convenient. `Shutdown()` destroys every live resource and logs leaks
(pool slots still alive) with their creation tags in dev builds.

**Threading.** One thread, the context thread, for everything (§5 FIXME). The contract is
documented so nobody "helpfully" calls `DrawQuad` from a job.

## 11. Milestones

- **M1.1 — RHI core.** Handles, generation pools, descriptors, `IRHIDevice`, backend factory,
  `GetSurfacePreferences`. Header-only compile test, no GL.
- **M1.2 — OpenGL backend.** Context extraction from the window class (kills the §9 FIXME),
  Glad init, debug callback → injected log, clear-color proof through the full stack.
- **M1.3 — RenderSystem + host adapter.** Lifecycle wired into the host loop, resize path.
- **M1.4 — Renderer2D batcher.** Shader, dynamic VBO, texture slots, `RenderStats` + overlay.
- **M1.5 — RenderView producers.** Math helper in the module; `CameraSystem` on the host side
  reading entt; second-view test (fixed debug camera) to prove multiplicity works.

## 12. Decisions log (short form)

| Decision | Alternative rejected | Why |
|---|---|---|
| Standalone module, contracts injected | Renderer as engine subsystem | Host-portability; dependency direction |
| Opaque handles + pools | Virtual resource classes | DOD, no per-resource heap/vtable, Vulkan-ready |
| RenderView POD per frame | Camera object inside renderer | Ownership, N views free, editor/game share path |
| Backend owns GL context + swap | Window owns context | Correct layering; Vulkan has no "window context" |
| Surface prefs query before window creation | Backend fixes up context after | GL profile is immutable post-creation |
| Single render thread now | Command buffer + render thread | No profile data justifying complexity yet |
