#include "RendererManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/Window/IWindowManager.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IEngine.h"

#include "Renderer/Config/Config_Renderer.h"

#include "Core/Events/EventBus.h"
#include "Core/Window/WindowEvents.h"

#include "RHI/RHIBackend.h"       // BackendFromString
#include "RHI/IGraphicsContext.h"
#include "RHI/Framebuffer.h"      // FramebufferSpec + the UniquePtr<IFramebuffer> deleter

#include "Renderer/RenderSystem.h"
#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"

#include "Renderer/ShaderSource.h"

#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Components/DummyComponent.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Config/Config_Engine.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"

namespace Opaax
{
    RendererManager::RendererManager()  = default;
    RendererManager::~RendererManager() = default;
    
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
        lDesc.Backend      = BackendFromString(lEngineCfg.RenderBackend);
        lDesc.Surface      = lSurface;
        lDesc.Width        = lWindow->GetWidth();
        lDesc.Height       = lWindow->GetHeight();
        lDesc.SpriteShader = ShaderSource::LoadShaderDescFromFile(lShaderPath);
        lDesc.ClearColor   = lRenderCfg.ClearColor;

        m_RenderSystem = MakeUnique<RenderSystem>();
        if (!m_RenderSystem->Init(lDesc))
        {
            OPAAX_LOG(LogRendererManager, Error, "RenderSystem failed to initialize")
            m_RenderSystem.reset();
            return false;
        }

        // React to window resize via the Tier-3 bus — replaces the per-frame size poll.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus()
            .Subscribe<WindowResize>(this, &RendererManager::HandleWindowResize);

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
    // Render — the frame, then the drain.
    //
    // The debug queue is strictly per-frame and its producers refill it every frame (the editor's
    // ViewportPanel enqueues in OnPreRender, before this runs). Clearing OUTSIDE RenderFrame is what
    // keeps a frame we could NOT render — no render core, zero-size target — from letting the queue
    // grow without bound: those early-outs skip the draw, never the drain.
    // =========================================================================
    void RendererManager::Render(double /*Alpha*/)
    {
        RenderFrame();
        m_DebugDraw.Clear();
    }

    void RendererManager::RenderFrame()
    {
        if (!m_RenderSystem)
        {
            return;
        }

        // The primary target decides where the world lands: the editor's offscreen FBO when set,
        // else the backbuffer (runtime default). Its size — not a cached window size — drives the
        // view, so an undocked/resized viewport rescales the render (D2: resize is inverted).
        IRenderTarget& lTarget = m_PrimaryTarget ? *m_PrimaryTarget : m_RenderSystem->GetBackbuffer();

        const Uint32 lWidth  = lTarget.GetWidth();
        const Uint32 lHeight = lTarget.GetHeight();
        if (lWidth == 0 || lHeight == 0) { return; }

        // Centered Y-up ortho: world (0,0) at target centre, 1 unit = 1px. A camera-view system
        // will produce this RenderView later; for now the adapter builds it.
        const float lHalfW = static_cast<float>(lWidth)  * 0.5f;
        const float lHalfH = static_cast<float>(lHeight) * 0.5f;
        RenderView lView;
        lView.ViewProjection = glm::ortho(-lHalfW, lHalfW, -lHalfH, lHalfH, -1.f, 1.f);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginFrame();
        m_RenderSystem->BeginPass(lTarget, lView);

        Renderer2D& lRenderer = m_RenderSystem->GetRenderer2D();

        // Draw the active world: one quad per DummyComponent (position / size / color).
        if (m_WorldManager != nullptr)
        {
            if (World* lWorld = m_WorldManager->GetActiveWorld())
            {
                lWorld->Each<DummyComponent>([&lRenderer](EntityID, DummyComponent& InComp)
                {
                    lRenderer.DrawQuad(InComp.Position, InComp.Size, InComp.Color);
                });
            }
        }

        // Debug overlay — each queued line as a thin rotated quad, so this reuses the world's batch
        // and adds no RHI/shader/vertex-layout surface. The Debug band sorts above world geometry
        // regardless of submission order, so no manual ordering is needed here.
        for (const DebugLine& lLine : m_DebugDraw.Lines())
        {
            const DebugQuad lQuad = ToQuad(lLine);
            lRenderer.DrawQuad(lQuad.Center, lQuad.Size, lLine.Color, lQuad.RotationRad, ERenderLayer::Debug);
        }

        m_RenderSystem->EndPass();
        m_RenderSystem->EndFrame();
    }
    
    void RendererManager::SetPrimaryRenderTarget(IRenderTarget* InTarget)
    {
        m_PrimaryTarget = InTarget;
        OPAAX_LOG(LogRendererManager, Info, "Primary render target set to {}", InTarget ? "offscreen" : "backbuffer")
    }
    
    UniquePtr<IFramebuffer> RendererManager::CreateFramebuffer(const FramebufferSpec& InSpec)
    {
        if (!m_RenderSystem)
        {
            OPAAX_LOG(LogRendererManager, Error, "CreateFramebuffer before the render core started — none created.")
            return nullptr;
        }

        return m_RenderSystem->CreateFramebuffer(InSpec);
    }

    void RendererManager::Present()
    {
        if (m_RenderSystem)
        {
            m_RenderSystem->Present();
        }
    }

    // =========================================================================
    // Bus handler — window resize (Tier-3). Forwards to the render core, which resizes the
    // backbuffer. Runs at the frame's Flush, before Render. When an offscreen primary target is
    // active its size is owned by its owner (the panel), independent of the window — this only
    // keeps the backbuffer current for the runtime / undocked path.
    // =========================================================================
    void RendererManager::HandleWindowResize(const WindowResize& InResize)
    {
        if (m_RenderSystem)
        {
            m_RenderSystem->Resize(InResize.Width, InResize.Height);
        }

        OPAAX_LOG(LogRendererManager, Trace, "Backbuffer resized to {}x{} (via event bus)", InResize.Width, InResize.Height)
    }
}
