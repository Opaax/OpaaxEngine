#include "RendererManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/Window/IWindowManager.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IStatsService.h"   // OPAAX_STAT_SCOPE — this subsystem opts in

#include "Renderer/Config/Config_Renderer.h"

#include "Core/Events/EventBus.h"
#include "Core/Window/WindowEvents.h"

#include "RHI/RHIBackend.h"       // BackendFromString
#include "RHI/IGraphicsContext.h"
#include "RHI/Framebuffer.h"      // FramebufferSpec + the TUniquePtr<IFramebuffer> deleter
#include "RHI/Texture.h"          // the TUniquePtr<ITexture2D> deleter

#include "Renderer/CameraView.h"
#include "Renderer/RenderSystem.h"
#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"

#include "Renderer/ShaderSource.h"
#include "Core/IO/FileIO.h"       // the host owns the read (see the shader load below)

#include "World/WorldManager.h"
#include "World/World.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

#include "Core/Maths/Maths.h"     // DegreesToRadians — the transform authors degrees, the renderer takes radians

#include "Engine/Subsystems/Resources/ResourceManager.h"          // Load<TextureResource> — the cache
#include "Engine/Subsystems/Resources/Types/TextureResource.h"

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
        const EngineConfigData&   lEngineCfg = lConfigSys.Get<Config_Engine>().GetData();
        const RendererConfigData& lRenderCfg = lConfigSys.Get<Config_Renderer>().GetData();

        Window*           lWindow  = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();
        IGraphicsContext* lSurface = lWindow ? lWindow->GetGraphicsContext() : nullptr;
        
        if (lSurface == nullptr)
        {
            OPAAX_LOG(LogRendererManager, Error, "No graphics context/surface for the render system");
            return false;
        }

        // Host reads the shader off disk (module never touches IPaths / file IO) — literally, now:
        // ShaderSource takes the TEXT and only parses/compiles it.
        const OpaaxString lShaderPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Sprite.glsl");

        const OpaaxString lShaderSrc = FileIO::ReadAllText(lShaderPath);
        if (lShaderSrc.IsEmpty())
        {
            OPAAX_LOG(LogRendererManager, Error, "cannot read shader file '{}'", lShaderPath.CStr());
        }

        RenderSystemDesc lDesc;
        lDesc.Backend      = ResolveSupportedBackend(lEngineCfg.Render.Backend);
        lDesc.Surface      = lSurface;
        lDesc.Width        = lWindow->GetWidth();
        lDesc.Height       = lWindow->GetHeight();
        lDesc.SpriteShader = ShaderSource::FromSource(lShaderSrc, lShaderPath);
        lDesc.ClearColor   = lRenderCfg.ClearColor;

        m_RenderSystem = MakeUnique<RenderSystem>();
        if (!m_RenderSystem->Init(lDesc))
        {
            OPAAX_LOG(LogRendererManager, Error, "RenderSystem failed to initialize");
            m_RenderSystem.reset();
            return false;
        }

        // React to window resize via the Tier-3 bus — replaces the per-frame size poll.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus()
            .Subscribe<WindowResize>(this, &RendererManager::HandleWindowResize);

        // Cache the world owner — Render draws whatever it reports as the active world.
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();
        m_Profiler     = OpaaxApplication::GetAppService<IStatsService>().GetProfiler();

        OPAAX_LOG(LogRendererManager, Info, "RendererManager started ({}x{})", lDesc.Width, lDesc.Height);
        return true;
    }

    void RendererManager::Shutdown()
    {
        // Unsubscribe BEFORE teardown — a late resize event must not reach a handler that
        // would touch a destroyed m_RenderSystem.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus().UnsubscribeAll(this);

        // Drop the texture claims FIRST. Shutdown order is the reverse of registration, so the
        // ResourceManager is still alive here to take the releases — and its own FlushAll, which
        // destroys the GPU handles, runs after this while the window's GL context is still up.
        m_TextureCache.clear();

        m_RenderSystem.reset(); // ~RenderSystem = WaitIdle + teardown while the window/context is alive
        OPAAX_LOG(LogRendererManager, Info, "RendererManager shutdown");
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
        {
            // Around RenderFrame only — the DebugDraw clear below is bookkeeping, not frame work,
            // and F4 requires it to run whether or not anything rendered.
            OPAAX_STAT_SCOPE(m_Profiler, "Renderer");
            RenderFrame();
        }

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

        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        // The world says WHERE it is looked at from; this adapter is what knows pixels, so it
        // composes the matrix. No world, or a world nobody produced a view for, falls back to the
        // default CameraView — the centred frame the engine drew before cameras existed.
        RenderView lView;
        lView.ViewProjection = MakeViewProjection(lWorld ? lWorld->GetCameraView() : CameraView{}, lWidth, lHeight);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginFrame();
        m_RenderSystem->BeginPass(lTarget, lView);

        Renderer2D& lRenderer = m_RenderSystem->GetRenderer2D();

        // Draw the active world: a solid quad per DummyComponent, a textured one per Sprite.
        if (lWorld != nullptr)
        {
            lWorld->Each<TransformComponent, DummyComponent>(
                [&lRenderer](EntityID, TransformComponent& InXf, DummyComponent& InComp)
                {
                    // Scale MULTIPLIES the component's own Size (③): the extent is what the thing
                    // is, the scale is what the transform does to it.
                    lRenderer.DrawQuad(InXf.Position, InComp.Size * InXf.Scale, InComp.Color,
                                       Maths::DegreesToRadians(InXf.Rotation));
                });

            DrawWorldSprites(*lWorld, lRenderer);
        }

        // Debug overlay — each queued line as a thin rotated quad, so this reuses the world's batch
        // and adds no RHI/shader/vertex-layout surface. The Debug band sorts above world geometry
        // regardless of submission order, so no manual ordering is needed here.
        for (const DebugLine& lLine : m_DebugDraw.GetLines())
        {
            const DebugQuad lQuad = ToQuad(lLine);
            // The LINE's band, not a hardcoded Debug: ③b's grid has to sit BEHIND world geometry,
            // and everything else still defaults to Debug and draws above it.
            lRenderer.DrawQuad(lQuad.Center, lQuad.Size, lLine.Color, lQuad.RotationRad, lLine.Layer);
        }

        m_RenderSystem->EndPass();
        m_RenderSystem->EndFrame();
    }
    
    void RendererManager::DrawWorldSprites(World& InWorld, Renderer2D& InRenderer)
    {
        InWorld.Each<TransformComponent, SpriteComponent>(
            [this, &InRenderer](EntityID, TransformComponent& InXf, SpriteComponent& InSprite)
            {
                if (!InSprite.bVisible)
                {
                    return;
                }

                // No texture named yet is a normal authoring state — a component just added, or one
                // whose image was cleared. Drawing a white quad for it would look like a bug in the
                // sprite; drawing nothing looks like what it is.
                ITexture2D* lTexture = ResolveTexture(InSprite.Texture);
                if (lTexture == nullptr)
                {
                    return;
                }

                InRenderer.DrawSprite(InXf.Position, InSprite.Size * InXf.Scale, *lTexture, InSprite.Color,
                                      Maths::DegreesToRadians(InXf.Rotation),
                                      InSprite.Layer, InSprite.OrderInLayer);
            });
    }

    ITexture2D* RendererManager::ResolveTexture(const TResourcePath<TextureResource>& InPath)
    {
        if (InPath.IsEmpty())
        {
            return nullptr;
        }

        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_TextureCache.find(lKey.GetId());
        if (lIt == m_TextureCache.end())
        {
            const OpaaxString lAbsolute = OpaaxApplication::GetAppService<IPaths>().AssetToAbsolute(InPath.Path);

            ResourceRef<TextureResource> lRef =
                OpaaxApplication::GetAppService<IEngine>().GetResources().Load<TextureResource>(lAbsolute.CStr());

            // Cached even when the load FAILED: the empty ref resolves to the magenta placeholder,
            // and keeping it stops a missing file from being retried once per sprite per frame.
            lIt = m_TextureCache.emplace(lKey.GetId(), Move(lRef)).first;

            // Logged ONCE per texture, on the branch that succeeded as well as the one that did
            // not — a cache that only reports failures is indistinguishable from one that never
            // ran. The two must not share a line: a failed ref resolves to the 2x2 placeholder, so
            // an unconditional "-> WxH" would cheerfully report a missing file as a 2x2 texture.
            if (const TextureResource* lLoaded = lIt->second.IsValid() ? lIt->second.Get() : nullptr)
            {
                OPAAX_LOG(LogRendererManager, Info, "Texture '{}' -> {}x{}",
                          InPath.Path.CStr(), lLoaded->Width, lLoaded->Height);
            }
            else
            {
                OPAAX_LOG(LogRendererManager, Warn, "Texture '{}' did not load — drawing the placeholder",
                          InPath.Path.CStr());
            }
        }

        TextureResource* lResource = lIt->second.Get();

        // Null while an async load is still in flight, or with no device at all — both mean "not
        // drawable this frame", and neither is worth a per-frame log line.
        return (lResource != nullptr) ? lResource->GetTexture() : nullptr;
    }

    void RendererManager::SetPrimaryRenderTarget(IRenderTarget* InTarget)
    {
        m_PrimaryTarget = InTarget;
        OPAAX_LOG(LogRendererManager, Info, "Primary render target set to {}", InTarget ? "offscreen" : "backbuffer");
    }
    
    TUniquePtr<IFramebuffer> RendererManager::CreateFramebuffer(const FramebufferSpec& InSpec)
    {
        if (!m_RenderSystem)
        {
            OPAAX_LOG(LogRendererManager, Error, "CreateFramebuffer before the render core started — none created.");
            return nullptr;
        }

        return m_RenderSystem->CreateFramebuffer(InSpec);
    }

    TUniquePtr<ITexture2D> RendererManager::CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        if (!m_RenderSystem)
        {
            OPAAX_LOG(LogRendererManager, Error, "CreateTexture before the render core started — none created.");
            return nullptr;
        }

        return m_RenderSystem->CreateTexture(InPixels, InWidth, InHeight, InChannels);
    }

    void RendererManager::Present()
    {
        if (m_RenderSystem)
        {
            m_RenderSystem->Present();
        }
    }

    void RendererManager::SetVSync(bool InEnabled)
    {
        if (m_RenderSystem)
        {
            m_RenderSystem->SetVSync(InEnabled);
        }
    }

    bool RendererManager::IsVSyncEnabled() const
    {
        return m_RenderSystem && m_RenderSystem->IsVSyncEnabled();
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

        OPAAX_LOG(LogRendererManager, Trace, "Backbuffer resized to {}x{} (via event bus)", InResize.Width, InResize.Height);
    }
}
