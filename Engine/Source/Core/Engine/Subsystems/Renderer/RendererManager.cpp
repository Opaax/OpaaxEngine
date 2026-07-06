#include "RendererManager.h"

#include "Core/Window.h"
#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/IWindowManager.h"
#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Config/Config_Engine.h"

#include "RHI/RenderAPI.h"
#include "RHI/IRenderAPI.h"
#include "RHI/ICommandBuffer.h"
#include "RHI/IGraphicsContext.h"
#include "Renderer/RenderTarget.hpp"

namespace Opaax
{
    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    RendererManager::RendererManager() = default;
    RendererManager::~RendererManager() = default;

    // =========================================================================
    // Getters
    // =========================================================================
    // Valid after Startup — m_RenderAPI owns the frame's recorder.
    ICommandBuffer& RendererManager::GetCommandBuffer() const
    {
        return m_RenderAPI->GetCommandBuffer();
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool RendererManager::Startup()
    {
        // Backend from engine config (string -> EBackend). Default is "OpenGL".
        const EngineConfigData& lConfig  = OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().Data();
        const EBackend          lBackend = RenderAPI::BackendFromString(lConfig.RenderBackend);
        OPAAX_LOG(LogRendererManager, Info, "Render backend = {}", RenderAPI::BackendToString(lBackend))

        m_RenderAPI = RenderAPI::Create(lBackend);
        if (!m_RenderAPI)
        {
            OPAAX_LOG(LogRendererManager, Error, "Backend '{}' produced no IRenderAPI", lConfig.RenderBackend.CStr())
            return false;
        }

        // The window already created + Init'd its graphics context (WindowManager runs during
        // InitializeApplication, before EngineStartup). The render API binds against it — Vulkan
        // borrows its device/swapchain; OpenGL ignores it (GL state is global).
        Window*           lWindow  = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();
        
        IGraphicsContext* lContext = lWindow ? lWindow->GetGraphicsContext() : nullptr;
        if (lContext == nullptr)
        {
            OPAAX_LOG(LogRendererManager, Error, "No graphics context available for the render API")
            return false;
        }
        m_RenderAPI->Init(*lContext);

        // Backbuffer target sized to the window. A per-frame size refresh (Render) stands in for
        // the not-yet-wired resize event system in the refresh path.
        m_Backbuffer = MakeUnique<DefaultRenderTarget>(lWindow->GetWidth(), lWindow->GetHeight());
        m_RenderAPI->SetViewport(0, 0, lWindow->GetWidth(), lWindow->GetHeight());

        OPAAX_LOG(LogRendererManager, Info, "RendererManager started ({}x{})", lWindow->GetWidth(), lWindow->GetHeight())
        return true;
    }

    void RendererManager::Shutdown()
    {
        // GPU-idle before dropping GPU resources (GL: glFinish). The graphics context is still
        // alive here — WindowManager tears it down later (reverse service-locator order).
        if (m_RenderAPI) { m_RenderAPI->WaitIdle(); }
        m_Backbuffer.reset();
        m_RenderAPI.reset();

        OPAAX_LOG(LogRendererManager, Info, "RendererManager shut down")
    }

    // =========================================================================
    // Frame — this pass just clears the backbuffer. Draw work (Renderer2D, passes)
    // records into GetCommandBuffer() between BeginFrame/EndFrame in a later pass;
    // the bracket lifts to Engine::Render then so peers share the frame.
    // =========================================================================
    void RendererManager::Render(double /*Alpha*/)
    {
        if (!m_RenderAPI || !m_Backbuffer) { return; }

        // Track the live window size each frame (no resize event system yet). Safe cast — the
        // manager created this backbuffer as a DefaultRenderTarget.
        if (Window* lWindow = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow())
        {
            static_cast<DefaultRenderTarget*>(m_Backbuffer.get())->OnResize(lWindow->GetWidth(), lWindow->GetHeight());
        }

        m_RenderAPI->BeginFrame();
        {
            ICommandBuffer& lCmd = m_RenderAPI->GetCommandBuffer();
            lCmd.BeginRenderPass(*m_Backbuffer, ELoadOp::Clear, m_ClearColor);
            lCmd.EndRenderPass();
        }
        m_RenderAPI->EndFrame();
    }
}
