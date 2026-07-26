# Plan — Editor M1 "Viewport"

> **Provenance:** designed on the **Fable** model (Plan agent) 2026-07-22, on the green tree at commit `29352e4`
> (RHI OpenGL-only via `IRHIDevice`, facade retired). Conforms to `Docs/Architectures/Editor.md` (D1–D10, §5 M1
> row) + `.claude/ARCHITECTURE.md` invariants (I1/I5, LC, **F2 present-split**, SE seams, X1). If any step
> disagrees with those, they win. This is the *execution strategy*; live tracking → `.claude/task/todo.md`.

---

## 0. Verified starting state (read, not assumed — L13/L14)

- `RenderSystem::BeginFrame()` currently hardcodes `BeginRenderPass(*m_Backbuffer, …)`; exposes `BeginScene/EndScene`
  ("scene" is retired vocab per F2).
- `IRHIDevice::BeginFrame()/EndFrame()` are **no-ops on OpenGL** — all real work is in `ICommandBuffer::BeginRenderPass/
  EndRenderPass`. So the `BeginPass/EndPass` reshape is a **reshuffle of existing bodies**, not new GPU logic.
- `ICommandBuffer::BeginRenderPass` already dispatches polymorphically on `IRenderTarget&` (`Bind()/Unbind()`) →
  **zero command-buffer changes** needed for M1.
- `RenderTarget.hpp` already earmarks itself for the M1 offscreen-target type.
- `RendererManager::Render()` computes the ortho view from a `m_ViewWidth/Height` cache fed **only** by `WindowResize` —
  this must instead read the *active render target's* size (D2: resize is inverted).
- **Present-split (S7) is already landed:** `EditorApplication::TickFrame()` = `BeginFrame(); Engine().Loop(); EndFrame();`
  and `RunApplication()` calls `Engine().Present()` once after `TickFrame()`. M1's remaining scope is narrower: offscreen
  target + panel + dockspace change only.
- Locator shutdown is provably reverse-registration-order; `IEditorService` is provided *after* `IEngine`
  (`OnProvideServices` fires end of Bootstrap) ⇒ `EditorService::OnShutdown()` runs **before** `Engine` shutdown, GL
  context + device still alive — load-bearing for FBO teardown (§3), and it needs **no new phase** (L7 gap-checked: none).

---

## 1. Context

Both `Sandbox.exe` and `SandboxEditor.exe` render the world straight to the GLFW backbuffer; the editor dockspace uses
`PassthruCentralNode` so the raw world shows through a transparent hole (M0 stopgap). M1 replaces that hole with a real
**dockable, resizable "Viewport" panel** displaying the world as a *texture* — the D2 output contract — decoupling render
resolution from window size. Every later editor milestone (M2 Panels, M4 PIE) assumes the world lives *inside* a panel.
The enabler is **F2 present-split**: `BeginFrame → BeginPass(target,view) → EndPass → EndFrame → Present`; only the
backbuffer is presented, offscreen targets never are — so the editor draws world→FBO, then UI→backbuffer, then one Present.

---

## 2. New types + engine changes (each checked vs invariants)

**2.1 `OffscreenRenderTarget`** (engine-side, `Renderer/RenderTarget.hpp`, beside `DefaultRenderTarget`) — an
`IRenderTarget` wrapping a **non-owning** `IFramebuffer*` (Bind/Unbind/GetWidth/GetHeight/`GetFramebuffer()`).
Engine-side because it's a generic renderer primitive (a runtime render-to-texture pass could reuse it), but it does NOT
own the FBO (I5). Add `#include "RHI/Framebuffer.h"` to the header. **I1:** no static — clean.

**2.2 `IEngine::SetPrimaryRenderTarget(IRenderTarget*)`** (`Application/Services/IEngine.h`) — `nullptr` = backbuffer
(runtime default, never called by Sandbox). Non-owning. `NullEngine` no-op (I3); `Engine` forwards to
`RendererManager` (I1 no static; I5 Engine stores nothing, forwards to the subsystem that uses it).

**2.2b `IEngine::Present()` → `IEngine::PresentBackbuffer()`** (rider on 2.2, same file, same edit pass). Bundled in
because M1 is the change that makes the old name ambiguous: once a *primary render target* exists, "present" no
longer obviously means "swap the OS window" — it could misread as "show whatever's primary." Renaming the
facade-level method makes §5's invariant self-evident from the call site (`Engine().PresentBackbuffer()` — the
one thing this milestone guarantees is that offscreen targets are never presented). Cascades to
`Engine::PresentBackbuffer() override`, `NullEngine::PresentBackbuffer()` (`IEngine.cpp`), and the one call site
(`OpaaxApplication.cpp:229`). **`RendererManager::Present()` / `RenderSystem::Present()` keep their name** —
unambiguous at that layer (single swapchain, standard D3D/Vulkan/GL term), so only the host-facing facade method
changes. No log strings reference "Present" (checked) — D4 byte-identical holds.

**2.3 `RendererManager`** — add `IRenderTarget* m_PrimaryTarget = nullptr` (non-owning) + `SetPrimaryRenderTarget()`
(stores + **logs**, L12). **Delete** `m_ViewWidth/m_ViewHeight`; `Render()` picks
`m_PrimaryTarget ? *m_PrimaryTarget : m_RenderSystem->GetBackbuffer()` and reads *its* size for the view; `OnWindowResized`
becomes a one-line forward to `m_RenderSystem->Resize()`. Runtime path stays byte-identical (nullptr → backbuffer size).

**2.4 `RenderSystem` F2 reshape** — `BeginScene/EndScene` → `BeginPass(IRenderTarget&, const RenderView&)/EndPass()`;
`BeginFrame/EndFrame` shrink to device open/close (the render-pass bracket moves into BeginPass/EndPass). Add
`IRenderTarget& GetBackbuffer()`. **No `ICommandBuffer` change.**

**2.5 `IEditorUIBackend::GetViewportImage(IFramebuffer&) → EditorViewportImage{Handle,UV0,UV1}`**
(`Editor/…/UI/IEditorUIBackend.h` + `OpenGLEditorUIBackend`) — direct port of the Legacy reference (GL FBO color id +
flipped V because GL is bottom-up). Not adding `GetTextureID` (M2 scope — simplicity gate).

**2.6 `EditorContext` grows `IEditorUIBackend& UIBackend`** (its doc already says it's a "growing set"). Requires
reordering `EditorService::Initialize()` to build `m_UIBackend` before `m_Context` (safe local reorder).

**2.7 `IEditorPanel` + `ViewportPanel`** (new `Editor/Source/Editor/Panels/`, auto-globbed). `IEditorPanel`: 4 virtuals
`Startup/Shutdown/OnPreRender/Draw` + `GetPanelName` (no event-bus coupling — that surface doesn't exist yet).
`ViewportPanel` owns `UniquePtr<IFramebuffer>` + `UniquePtr<OffscreenRenderTarget>`; **`Draw()` measures** the panel's
content region and caches a pending resize; **`OnPreRender()` applies** it next frame *before* the world renders. The
one-frame lag is normal for ImGui render-to-texture — flag it in the demo gate so it isn't mistaken for a bug.

**2.8 Framebuffer creation** — keep the free `IFramebuffer::Create` factory (Fork 1), `// FIXME` in `Framebuffer.h`.

---

## 3. Ownership & lifetime (I5 / LC)

FBO owned by `ViewportPanel` (owned by `EditorService`) — both inside `OpaaxEditorLib`, never touched by the engine; the
engine holds only a **non-owning `IRenderTarget*`** (`RendererManager::m_PrimaryTarget`). Resize ordering:
`EditorService::BeginFrame()` runs `ImGui::NewFrame(); m_ViewportPanel->OnPreRender();` **before** `Engine().Loop()`, so
the FBO is correct-sized before `Render()` reads it. **Teardown:** `EditorService::OnShutdown()` runs before `Engine`
shutdown (reverse registration order, proven) → `ViewportPanel::Shutdown()` calls
`Engine.SetPrimaryRenderTarget(nullptr)` **first** (while Engine alive), **then** frees the FBO (GL context still
current). No dangling pointer reaches a live frame; no defensive null-check in `RendererManager`. Runtime: never called,
destructor path identical to today (D4 byte-identical).

---

## 4. Seam routing (SE) — **no new seam**

Reuses two seams `EditorApplication` already overrides: **`PostEngineStartup`** → `EditorService::Initialize()` grows to
reorder (UIBackend before Context) + construct/Startup the panel; **`TickFrame`** → `BeginFrame()` grows to call
`OnPreRender()`, `EndFrame()` grows to call `Draw()` + drop `PassthruCentralNode`. `EditorService::OnShutdown()`
(IAppService lifecycle) shuts the panel down first. `Engine`/`OpaaxApplication` untouched except the one generic
`IEngine::SetPrimaryRenderTarget` (zero editor vocabulary — D4).

---

## 5. Present-split subtlety (F2), traced

Per editor frame: (1) `BeginFrame()` → `NewFrame(); OnPreRender()`. (2) `Engine().Loop()` → `Render()` →
`BeginPass(offscreen,view)` → `OffscreenRenderTarget::Bind()` → `glBindFramebuffer(FBO)` → world lands in the FBO, GL
fb 0 untouched → `EndPass()` → `Unbind()` → `glBindFramebuffer(0)`. (3) `EndFrame()` → `DrawDockspace()` (non-passthru) +
`ViewportPanel::Draw()` (samples FBO via `ImGui::Image`) + `ImGui::Render()` + `RenderDrawData()` — **UI draws into fb 0,
already restored by step 2's Unbind — no extra "rebind backbuffer" code needed.** (4) `RunApplication` calls `Present()`
once. This is the one easy-to-get-wrong subtlety.

---

## 6. Steps (each builds green; risk escalates)

- **S0 — Re-verify baseline** (no code): 3 presets `OPAAX_BUILD_OK` + test 80/339, 2 skipped (L13/L14).
- **S1 — Engine render-target plumbing** (runtime-only paths): `OffscreenRenderTarget`, `IEngine::SetPrimaryRenderTarget`
  (+NullEngine), `Engine` forward, `RenderSystem` F2 reshape + `GetBackbuffer()`, `RendererManager` target-selection +
  delete size cache, **`IEngine::Present`→`PresentBackbuffer` rename** (§2.2b, +`OpaaxApplication.cpp` call site).
  *Gate:* `Sandbox.exe` still 3 quads, log unchanged (D4). *Test:* new `RenderTargetTests.cpp` with a
  `StubFramebuffer` double (no GL) → suite **80→81**; add to `Engine/Tests/CMakeLists.txt` explicit list.
- **S2 — Editor UI backend + `EditorContext` growth** (additive + the `Initialize()` reorder). *Gate:* `SandboxEditor.exe`
  boots identical to M0 (still passthru, S11 RouteInput log fires).
- **S3 — `IEditorPanel` + `ViewportPanel` types** (new files, not wired). *Gate:* build green only.
- **S4 — Wire panel into `EditorService`; drop dockspace passthru (milestone-delivering).** *Demo (observable, L12):*
  a dockable **"Viewport"** panel shows the 3 quads; resizing/undocking it rescales the render; log has
  `RendererManager` "primary target set to offscreen" once + `ViewportPanel` "resized to WxH" per resize. *Regression:*
  `Sandbox.exe` identical to S1. *Test:* engine suite unaffected (editor lib not linked into tests).

---

## 7. Forks (decide up front — CH)

1. **`IFramebuffer::Create` free factory vs `IRHIDevice::CreateFramebuffer`?** → **Keep the free factory.** Routing
   through the device would force a new `IEngine::GetRenderDevice()` exposure purely so the editor can reach the device
   (device is engine-internal). Free factory already works standalone; punt is reversible + naturally forced when a
   `VulkanRHIDevice` needs device-owned FBOs. `// FIXME` in `Framebuffer.h`.
2. **`SetPrimaryRenderTarget` on `IEngine` vs `RenderSystem`/`RendererManager`?** → **`IEngine`.** RenderSystem is a
   portable core with no host reach-back; RendererManager is never exposed via `IEngine`'s getters. `IEngine` is the only
   render-adjacent surface the editor can reach; anywhere else needs a *new* accessor anyway (more surface). Matches I4.
3. **Viewport camera/view for M1?** → **Keep the existing hardcoded centered Y-up ortho (1u=1px)**, re-parameterized off
   the target size — no camera type exists yet, so building one now balloons the milestone (L3). `RenderView` is already
   the seam for what comes later: its own doc says the host "composes the matrices and hands over a snapshot... split-screen
   / editor viewport / minimap are just more views." M1's target/panel plumbing (§2.1–2.7) only decides *where* pixels
   land, never *what* matrix produces them, so it doesn't need to change shape when a camera lands. Confirmed split for
   later: the **Editor owns its own camera** (edit mode); **PIE starts from the World's (game) camera** — two different
   sources feeding the same `RenderView` snapshot, selected by whoever is driving the panel that frame. Out of scope here.
- **Minor:** introduce `IEditorPanel` now, minimal (M2 needs a common panel base anyway); write fresh, don't migrate
  Legacy's event-bus-coupled version.

---

## 8. Verification

1. **3 presets** grep `OPAAX_BUILD_OK` (debug-editor / release / release-editor).
2. **Tests** `./build.bat test` → **81 cases** (80 + OffscreenRenderTarget), 339 + new suite's assertions, 2 skipped
   (record exact after first run).
3. **`Sandbox.exe`** unchanged — 3 quads, same log (D4 byte-identical).
4. **`SandboxEditor.exe`** (observable, L12): dockable "Viewport" panel with the 3 quads; resize rescales the render; log
   `RendererManager` "primary target set" once + `ViewportPanel` "resized WxH" per resize; 0 err / 0 warn.

## 9. Critical files
- Engine: `Renderer/RenderTarget.hpp`, `Renderer/RenderSystem.{h,cpp}`, `Application/Services/IEngine.{h,cpp}`,
  `Engine/Engine.{h,cpp}`, `Engine/Subsystems/Renderer/RendererManager.{h,cpp}`, `Application/OpaaxApplication.cpp`
  (Present→PresentBackbuffer call site), `Engine/Tests/Renderer/RenderTargetTests.cpp` (new) + `Engine/Tests/CMakeLists.txt`.
- Editor: `Editor/…/UI/IEditorUIBackend.h` + `OpenGLEditorUIBackend.{h,cpp}`, `Editor/…/EditorContext.h`,
  `Editor/…/Panels/IEditorPanel.h` (new), `Editor/…/Panels/ViewportPanel.{h,cpp}` (new), `Editor/…/EditorService.{h,cpp}`.
- Reference only (X1, do not modify/depend): `Legacy/Editor/Panels/ViewportPanel.*`, `Legacy/Editor/UI/OpenGLEditorUIBackend.cpp`.
