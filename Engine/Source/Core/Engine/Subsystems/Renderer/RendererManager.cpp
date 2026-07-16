#include "RendererManager.h"

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/Window/IWindowManager.h"
#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Config/Config_Engine.h"
#include "Core/Config/Config_Renderer.h"
#include "Core/Application/Services/IEngine.h"
#include "Core/Events/EventBus.h"
#include "Core/Window/WindowEvents.h"

#include "RHI/RenderAPI.h"        // BackendFromString
#include "RHI/RenderLog.h"        // ERenderLogLevel
#include "RHI/IGraphicsContext.h"

#include "Renderer/RenderSystem.h"
#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/ShaderSource.h"

#include "Core/World/WorldManager.h"
#include "Core/World/World.h"
#include "Core/Components/DummyComponent.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Engine/Subsystems/EventBus/EngineEventBus.h"

namespace Opaax
{
    namespace
    {
        // Bridge the module's injected log onto the engine logger — the ONLY place render-module
        // output crosses back into the host's logging. The RenderSystem itself never calls OPAAX_LOG.
        void RenderLogShim(ERenderLogLevel InLevel, const char* InMsg)
        {
            const char* lMsg = InMsg ? InMsg : "";
            switch (InLevel)
            {
                case ERenderLogLevel::Trace: OPAAX_LOG(LogRendererManager, Trace, "{}", lMsg) break;
                case ERenderLogLevel::Info:  OPAAX_LOG(LogRendererManager, Info,  "{}", lMsg) break;
                case ERenderLogLevel::Warn:  OPAAX_LOG(LogRendererManager, Warn,  "{}", lMsg) break;
                case ERenderLogLevel::Error: OPAAX_LOG(LogRendererManager, Error, "{}", lMsg) break;
            }
        }
    }

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    RendererManager::RendererManager()  = default;
    RendererManager::~RendererManager() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool RendererManager::Startup()
    {
        // Resolve host state (the adapter's job) and pack it into a plain desc for the module.
        IConfigSystem&            lConfigSys = OpaaxApplication::GetAppService<IConfigSystem>();
        const EngineConfigData&   lEngineCfg = lConfigSys.Get<Config_Engine>().Data();
        const RendererConfigData& lRenderCfg = lConfigSys.Get<Config_Renderer>().Data();

        Window*           lWindow  = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();
        IGraphicsContext* lSurface = lWindow ? lWindow->GetGraphicsContext() : nullptr;
        if (lSurface == nullptr)
        {
            OPAAX_LOG(LogRendererManager, Error, "No graphics context/surface for the render system")
            return false;
        }

        // Host reads the shader off disk (module never touches IPaths / file IO).
        const OpaaxString lShaderPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Sprite.glsl");

        RenderSystemDesc lDesc;
        lDesc.Backend      = RenderAPI::BackendFromString(lEngineCfg.RenderBackend);
        lDesc.Surface      = lSurface;
        lDesc.Width        = lWindow->GetWidth();
        lDesc.Height       = lWindow->GetHeight();
        lDesc.Log          = &RenderLogShim;
        lDesc.SpriteShader = ShaderSource::LoadShaderDescFromFile(lShaderPath);
        lDesc.ClearColor   = lRenderCfg.ClearColor;

        m_RenderSystem = MakeUnique<RenderSystem>();
        if (!m_RenderSystem->Init(lDesc))
        {
            OPAAX_LOG(LogRendererManager, Error, "RenderSystem failed to initialize")
            m_RenderSystem.reset();
            return false;
        }

        // Seed the cached viewport with the initial size; the bus keeps it current.
        m_ViewWidth  = lDesc.Width;
        m_ViewHeight = lDesc.Height;

        // React to window resize via the Tier-3 bus — replaces the per-frame size poll.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus()
            .Subscribe<WindowResize>(this, &RendererManager::OnWindowResized);

        // Cache the world owner — Render draws whatever it reports as the active world.
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();

        OPAAX_LOG(LogRendererManager, Info, "RendererManager started ({}x{})", lDesc.Width, lDesc.Height)
        return true;
    }

    void RendererManager::Shutdown()
    {
        // Unsubscribe BEFORE teardown — a late resize event must not reach a handler that
        // would touch a destroyed m_RenderSystem.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus().UnsubscribeAll(this);

        m_RenderSystem.reset(); // ~RenderSystem = WaitIdle + teardown while the window/context is alive
        OPAAX_LOG(LogRendererManager, Info, "RendererManager shutdown")
    }

    // =========================================================================
    // Frame — drive the module: resize (polled), then a single scene with the test quad.
    // The BeginFrame/EndFrame(+Present) bracket owns the frame; the app loop no longer swaps.
    // =========================================================================
    void RendererManager::Render(double /*Alpha*/)
    {
        if (!m_RenderSystem)
        {
            return;
        }

        // Viewport size is bus-driven (OnWindowResized) — no per-frame polling.
        const Uint32 lWidth  = m_ViewWidth;
        const Uint32 lHeight = m_ViewHeight;
        if (lWidth == 0 || lHeight == 0) { return; }

        // Centered Y-up ortho: world (0,0) at screen centre, 1 unit = 1px. A camera-view system
        // will produce this RenderView later; for now the adapter builds it.
        const float lHalfW = static_cast<float>(lWidth)  * 0.5f;
        const float lHalfH = static_cast<float>(lHeight) * 0.5f;
        RenderView lView;
        lView.ViewProjection = glm::ortho(-lHalfW, lHalfW, -lHalfH, lHalfH, -1.f, 1.f);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginFrame();
        m_RenderSystem->BeginScene(lView);

        // Draw the active world: one quad per DummyComponent (position / size / color).
        if (m_WorldManager != nullptr)
        {
            if (World* lWorld = m_WorldManager->GetActiveWorld())
            {
                Renderer2D& lRenderer = m_RenderSystem->GetRenderer2D();
                lWorld->Each<DummyComponent>([&lRenderer](EntityID, DummyComponent& InComp)
                {
                    lRenderer.DrawQuad(InComp.Position, InComp.Size, InComp.Color);
                });
            }
        }

        m_RenderSystem->EndScene();
        m_RenderSystem->EndFrame();
    }

    // =========================================================================
    // Bus handler — window resize (Tier-3). Updates the cached viewport + resizes
    // the render core. Runs at the frame's Flush, before Render.
    // =========================================================================
    void RendererManager::OnWindowResized(const WindowResize& InResize)
    {
        m_ViewWidth  = InResize.Width;
        m_ViewHeight = InResize.Height;

        if (m_RenderSystem)
        {
            m_RenderSystem->Resize(m_ViewWidth, m_ViewHeight);
        }

        OPAAX_LOG(LogRendererManager, Trace, "Viewport resized to {}x{} (via event bus)", m_ViewWidth, m_ViewHeight)
    }
}
