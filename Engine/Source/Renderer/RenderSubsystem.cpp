#include "RenderSubsystem.h"

#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/RenderAPI.h"
#include "RHI/IGraphicsContext.h"
#include "Renderer/Pass/WorldRenderPass.h"
#include "Renderer/Pass/OverlayRenderPass.h"
#include "Renderer/Systems/RenderStatsOverlaySystem.h"
#include "Renderer/Camera/OrthographicCamera.h"
#include "World/IOverlayRenderSystem.h"
#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool RenderSubsystem::Startup()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Startup()");

        // Backend comes from engine config (string -> EBackend), not hardcoded.
        // Context bootstrap (make-current + glad load) already happened in WindowsWindow::Init
        // via the backend's IGraphicsContext — glad is confined there, not loaded here.
        const EBackend lBackend = RenderAPI::BackendFromString(EngineConfig::RenderBackend());
        OPAAX_CORE_INFO("RenderSubsystem: render backend = {}", RenderAPI::BackendToString(lBackend));

        // RenderCommand takes ownership of the raw ptr (documented on RenderCommand);
        // .release() hands the UniquePtr's payload over to that ownership contract.
        UniquePtr<IRenderAPI> lAPI = RenderAPI::Create(lBackend);
        if (!lAPI)
        {
            OPAAX_CORE_ERROR("RenderSubsystem: backend '{}' produced no IRenderAPI.",
                EngineConfig::RenderBackend());
            return false;
        }

        // The render API binds against the window's graphics context (Vulkan borrows its
        // device/swapchain; OpenGL ignores it). The window created + Init'd the context already.
        IGraphicsContext* lContext = GetEngineApp() ? GetEngineApp()->GetWindow().GetGraphicsContext()
                                                    : nullptr;
        if (!lContext)
        {
            OPAAX_CORE_ERROR("RenderSubsystem: no graphics context available for the render API.");
            return false;
        }
        RenderCommand::Init(lAPI.release(), *lContext);

        Renderer2D::Init();

        // Register the built-in passes (registration order = execution order).
        // Passes hold the engine app by pointer (IoC) and re-fetch volatile state at Execute.
        // World first (clears + draws the scene), then Overlay (screen-space, composites on top).
        m_Pipeline.AddPass(MakeUnique<WorldRenderPass>(GetEngineApp()));
        m_Pipeline.AddPass(MakeUnique<OverlayRenderPass>(GetEngineApp()));

        // Engine-owned render-stats overlay — registered only when enabled in config (no runtime key
        // yet; a live toggle is a future CVar/console milestone). Reports engine state, so the engine
        // registers it, not game code.
        if (EngineConfig::RenderStats())
        {
            RegisterOverlaySystem(MakeUnique<RenderStatsOverlaySystem>());
            OPAAX_CORE_INFO("RenderSubsystem: render-stats overlay enabled (render.stats=true).");
        }

        // Active 2D camera (folded in from CameraSubsystem): default orthographic camera owned here.
        // CPU-side, independent of the device; the render passes fetch GetActiveCamera() at Execute.
        if (!m_OwnedCamera) { m_OwnedCamera = MakeUnique<OrthographicCamera>(); }
        m_ActivePtr = m_OwnedCamera.get();

        // Set initial viewport + seed the camera projection from the window size.
        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            RenderCommand::SetViewport(0, 0, lWindow.GetWidth(), lWindow.GetHeight());
            m_LastViewportWidth  = lWindow.GetWidth();
            m_LastViewportHeight = lWindow.GetHeight();
            m_ActivePtr->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
        }

        return true;
    }
 
    void RenderSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Shutdown()");
        // NOTE: the GPU-idle barrier is CoreEngineApp::Shutdown's single authoritative WaitIdle
        //   (runs before ANY GPU teardown, incl. assets). No per-subsystem wait needed here.
        m_ActivePtr = nullptr;       // camera (folded in) — CPU-only; the editor cleared its non-owning
        m_OwnedCamera.reset();       // pointer in its own (earlier) Shutdown, so this drops only the owned one
        m_OverlaySystems.clear();    // drop overlay systems (+ any GPU resources they own) before the device
        m_Pipeline.Clear();          // drop passes before the render API/Renderer2D go away
        Renderer2D::Shutdown();
        RenderCommand::Shutdown();
    }
 
    bool RenderSubsystem::OnEvent(OpaaxEvent& Event)
    {
        OpaaxEventDispatcher lDispatcher(Event);
        lDispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& E) { return OnWindowResize(E); });
        return false;
    }
 
    bool RenderSubsystem::OnWindowResize(WindowResizeEvent& Event)
    {
        RenderCommand::SetViewport(0, 0, Event.GetWidth(), Event.GetHeight());
#if !OPAAX_WITH_EDITOR
        // Release: the window IS the render target, so its size drives the camera projection. In editor
        // builds the ViewportPanel is the target and pushes its own size via SetViewportSize — the window
        // dims would race the FBO (the editor render target is the FBO, not the window).
        SetViewportSize(Event.GetWidth(), Event.GetHeight());
#endif
        return false;
    }

    void RenderSubsystem::RegisterOverlaySystem(UniquePtr<IOverlayRenderSystem> InSystem)
    {
        if (InSystem)
        {
            m_OverlaySystems.push_back(Move(InSystem));
        }
    }

    // =============================================================================
    // Active camera (folded in from the former CameraSubsystem) — dual-slot ownership
    // =============================================================================
    void RenderSubsystem::SetActiveCamera(UniquePtr<ICamera> InCamera)
    {
        if (!InCamera)
        {
            OPAAX_CORE_WARN("RenderSubsystem::SetActiveCamera - null camera ignored.");
            return;
        }

        // Owning swap always destroys whatever was previously owned, even if a non-owning camera is
        // currently active — the prior owned camera is structurally inaccessible once the new one moves in.
        m_OwnedCamera = Move(InCamera);
        m_ActivePtr   = m_OwnedCamera.get();
        if (m_LastViewportWidth > 0 && m_LastViewportHeight > 0)
        {
            m_ActivePtr->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
        }
        OPAAX_CORE_INFO("RenderSubsystem - active camera swapped (owning).");
    }

    void RenderSubsystem::SetActiveCameraNonOwning(ICamera* InCamera)
    {
        m_ActivePtr = InCamera;
        if (InCamera)
        {
            if (m_LastViewportWidth > 0 && m_LastViewportHeight > 0)
            {
                InCamera->SetViewportSize(m_LastViewportWidth, m_LastViewportHeight);
            }
            OPAAX_CORE_INFO("RenderSubsystem - active camera swapped (non-owning).");
        }
        else
        {
            OPAAX_CORE_INFO("RenderSubsystem - active camera cleared.");
        }
    }

    ICamera& RenderSubsystem::GetActiveCamera()
    {
        return *m_ActivePtr;
    }

    void RenderSubsystem::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        m_LastViewportWidth  = InWidth;
        m_LastViewportHeight = InHeight;
        if (m_ActivePtr)
        {
            m_ActivePtr->SetViewportSize(InWidth, InHeight);
        }
    }
 
} // namespace Opaax