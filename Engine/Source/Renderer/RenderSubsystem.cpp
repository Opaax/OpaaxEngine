#include "RenderSubsystem.h"

#include "Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/RenderAPI.h"
#include "Renderer/Pass/WorldRenderPass.h"
#include "Renderer/Pass/OverlayRenderPass.h"
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
        RenderCommand::Init(lAPI.release());

        Renderer2D::Init();

        // Register the built-in passes (registration order = execution order).
        // Passes hold the engine app by pointer (IoC) and re-fetch volatile state at Execute.
        // World first (clears + draws the scene), then Overlay (screen-space, composites on top).
        m_Pipeline.AddPass(MakeUnique<WorldRenderPass>(GetEngineApp()));
        m_Pipeline.AddPass(MakeUnique<OverlayRenderPass>(GetEngineApp()));

        // Set initial viewport
        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            RenderCommand::SetViewport(0, 0, lWindow.GetWidth(), lWindow.GetHeight());
        }

        return true;
    }
 
    void RenderSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("RenderSubsystem::Shutdown()");
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
        return false;
    }
 
} // namespace Opaax