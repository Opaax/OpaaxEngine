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
#include "Engine/Subsystems/Resources/Types/SpriteSheetResource.h" // the sheet a sprite may name instead
#include "Engine/Subsystems/Resources/Types/FontFaceResource.h"    // the baked atlas a text draws from
#include "Engine/Subsystems/Resources/Types/FontFamilyResource.h"  // the family a text may name instead

#include "Renderer/Text/Text2D.h"
#include "World/Components/TextComponent.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"

namespace Opaax
{
    namespace
    {
        /**
         * The four style axes as one integer, so a (family, style) pair keys a one-shot warning set.
         *
         * Every axis is small — nine subsets, nine weights, four widths, two slants — so a byte each
         * is room to spare and the packing cannot collide.
         */
        Uint32 PackStyle(const FontStyleKey& InKey) noexcept
        {
            return (static_cast<Uint32>(InKey.Subset)              << 24)
                 | ((WeightValue(InKey.Weight) / 100u)             << 16)
                 | (static_cast<Uint32>(InKey.Width)               << 8)
                 |  static_cast<Uint32>(InKey.Slant);
        }

        /**
         * The face a family answers with when it has nothing in the requested script.
         *
         * A VIEW ONTO THIS rather than an invalid one, because drawing nothing would make the wrong
         * subset look like the wrong position, the wrong colour, or a component nobody wired — four
         * indistinguishable bugs. A row of boxes says exactly one thing, and it is the true one.
         */
        const FontFaceData& TofuFace() noexcept
        {
            static const FontFaceData s_Tofu = FontFaceData::Tofu();
            return s_Tofu;
        }
    }

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

        lDesc.Limits.MaxQuads        = lRenderCfg.MaxQuadsPerBatch;
        lDesc.Limits.MaxTextureSlots = lRenderCfg.MaxTextureSlots;

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

        // Drop the resource claims FIRST. Shutdown order is the reverse of registration, so the
        // ResourceManager is still alive here to take the releases — and its own FlushAll, which
        // destroys the GPU handles, runs after this while the window's GL context is still up.
        m_TextureCache.clear();
        m_SheetCache.clear();
        m_FaceCache.clear();     // holds the R8 atlases — same GL-context deadline as the textures
        m_FamilyCache.clear();

        m_RenderSystem.reset(); // ~RenderSystem = WaitIdle + teardown while the window/context is alive
        OPAAX_LOG(LogRendererManager, Info, "RendererManager shutdown");
    }
    
    // =========================================================================
    // Render — the frame, then the drain.
    //
    // The debug queue and the submitted views are both strictly per-frame, and their producers
    // refill them every frame (the editor's ViewportPanel does both in OnPreRender, before this
    // runs). Clearing OUTSIDE RenderFrame is what keeps a frame we could NOT render — no render
    // core, zero-size target — from letting either grow without bound: those early-outs skip the
    // draw, never the drain.
    // =========================================================================
    void RendererManager::Render(double /*Alpha*/)
    {
        {
            // Around RenderFrame only — the two clears below are bookkeeping, not frame work, and
            // F4 requires them to run whether or not anything rendered.
            OPAAX_STAT_SCOPE(m_Profiler, "Renderer");
            RenderFrame();
        }

        SubmitRenderCounters();

        m_DebugDraw.Clear();
        m_SubmittedViews.clear();
    }

    void RendererManager::SubmitRenderCounters()
    {
        // The GPU reading goes in whether or not the profiler is attached... except that with stats
        // off there is nothing to submit to either, so one guard covers both.
        if (m_Profiler == nullptr) { return; }

        OpaaxApplication::GetAppService<IStatsService>().SubmitGpuMs(
            m_RenderSystem ? m_RenderSystem->GetGpuFrameTimeMs() : -1.0);

        // OUTSIDE RenderFrame's early-outs, so a frame that drew nothing reports zeros rather than
        // leaving the previous frame's numbers on screen — the same reason the DebugDraw clear is
        // out here (F4).
        const Renderer2DStats lStats = m_RenderSystem
                                           ? m_RenderSystem->GetRenderer2D().GetStats()
                                           : Renderer2DStats{};

        // Translated into NAMED counters here, at the adapter, so Core never learns what a draw call
        // is and the Stats panel needs no renderer type to display them.
        m_Profiler->AddCount("Draw Calls",    lStats.DrawCalls);
        m_Profiler->AddCount("Quads",         lStats.Quads);
        m_Profiler->AddCount("Texture Slots", lStats.PeakTextureSlots);
    }

    // =========================================================================
    // RenderFrame — one device frame, N passes.
    //
    // The runtime path is a SUBMISSION rather than a branch: with nothing submitted the frame is the
    // backbuffer framed by the active world, which is byte for byte what this drew before views
    // could be submitted at all. One rule, one loop, nothing to keep in step.
    // =========================================================================
    void RendererManager::RenderFrame()
    {
        if (!m_RenderSystem)
        {
            return;
        }

        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (m_SubmittedViews.empty())
        {
            // A world nobody produced a view for falls back to the default CameraView — the centred
            // frame the engine drew before cameras existed (CAM1).
            SubmitRenderView(m_RenderSystem->GetBackbuffer(),
                             lWorld != nullptr ? lWorld->GetCameraView() : CameraView{},
                             /*bInDrawOverlays*/ true);
        }

        // Counted BEFORE the device frame opens, so a frame with nothing drawable opens none — the
        // early-out this has always had, now per target rather than per frame.
        Uint32 lPasses = 0;

        for (const RenderPassRequest& lRequest : m_SubmittedViews)
        {
            if (lRequest.Target != nullptr && lRequest.Target->GetWidth() > 0 && lRequest.Target->GetHeight() > 0)
            {
                ++lPasses;
            }
        }

        if (lPasses == 0) { return; }

        ReportPassCount(lPasses);

        m_RenderSystem->BeginFrame();

        for (const RenderPassRequest& lRequest : m_SubmittedViews)
        {
            if (lRequest.Target == nullptr || lRequest.Target->GetWidth() == 0 || lRequest.Target->GetHeight() == 0)
            {
                continue;
            }

            RenderPass(*lRequest.Target, lWorld, lRequest.View, lRequest.bDrawOverlays);
        }

        m_RenderSystem->EndFrame();
    }

    void RendererManager::RenderPass(IRenderTarget& InTarget, World* InWorld, const CameraView& InView, bool bInDrawOverlays)
    {
        // The target's size — not a cached window size — drives the view, so an undocked/resized
        // viewport rescales the render (D2: resize is inverted).
        const Uint32 lWidth  = InTarget.GetWidth();
        const Uint32 lHeight = InTarget.GetHeight();

        // The submitter says WHERE it is looked at from; this adapter is what knows pixels, so it
        // composes the matrix.
        RenderView lView;
        lView.ViewProjection = MakeViewProjection(InView, lWidth, lHeight);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginPass(InTarget, lView);

        Renderer2D& lRenderer = m_RenderSystem->GetRenderer2D();

        // Draw the active world: a solid quad per DummyComponent, a textured one per Sprite.
        if (InWorld != nullptr)
        {
            InWorld->Each<TransformComponent, DummyComponent>(
                [&lRenderer](EntityID, TransformComponent& InXf, DummyComponent& InComp)
                {
                    // Scale MULTIPLIES the component's own Size (③): the extent is what the thing
                    // is, the scale is what the transform does to it.
                    lRenderer.DrawQuad(InXf.Position, InComp.Size * InXf.Scale, InComp.Color,
                                       Maths::DegreesToRadians(InXf.Rotation));
                });

            DrawWorldSprites(*InWorld, lRenderer);
            DrawWorldTexts(*InWorld, lRenderer);
        }

        // Debug overlay — each queued line as a thin rotated quad, so this reuses the world's batch
        // and adds no RHI/shader/vertex-layout surface. The Debug band sorts above world geometry
        // regardless of submission order, so no manual ordering is needed here.
        //
        // READ per pass, CLEARED once per frame (Render): two views that both want overlays each
        // draw them, which is what a second authoring view would expect.
        if (bInDrawOverlays)
        {
            for (const DebugLine& lLine : m_DebugDraw.GetLines())
            {
                const DebugQuad lQuad = ToQuad(lLine);
                // The LINE's band, not a hardcoded Debug: ③b's grid has to sit BEHIND world geometry,
                // and everything else still defaults to Debug and draws above it.
                lRenderer.DrawQuad(lQuad.Center, lQuad.Size, lLine.Color, lQuad.RotationRad, lLine.Layer);
            }

            // Boxes are ONE hollow quad each, not four thin ones — same band rule as the lines.
            for (const DebugBox& lBox : m_DebugDraw.GetBoxes())
            {
                lRenderer.DrawQuadOutline(lBox.Center, lBox.Size, lBox.Color, lBox.Thickness,
                                          0.f, lBox.Layer);
            }
        }

        m_RenderSystem->EndPass();
    }

    void RendererManager::ReportPassCount(Uint32 InPasses)
    {
        if (InPasses <= 1 || m_bMultiPassLogged)
        {
            return;
        }

        m_bMultiPassLogged = true;

        OPAAX_LOG(LogRendererManager, Info, "Frame composed of {} render passes — the first multi-view frame", InPasses);
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

                ITexture2D*  lTexture = nullptr;
                SpriteUVRect lUV;

                if (!ResolveSpriteDraw(InSprite, lTexture, lUV))
                {
                    return;
                }

                InRenderer.DrawSprite(InXf.Position, InSprite.Size * InXf.Scale, *lTexture, InSprite.Color,
                                      Maths::DegreesToRadians(InXf.Rotation),
                                      InSprite.Layer, InSprite.OrderInLayer,
                                      lUV.UVMin, lUV.UVMax);
            });
    }

    void RendererManager::DrawWorldTexts(World& InWorld, Renderer2D& InRenderer)
    {
        InWorld.Each<TransformComponent, TextComponent>(
            [this, &InRenderer](EntityID, TransformComponent& InXf, TextComponent& InText)
            {
                if (!InText.bVisible || InText.Text.IsEmpty())
                {
                    return;
                }

                const FontFaceView lFace = ResolveTextDraw(InText);
                if (!lFace.IsValid())
                {
                    return;
                }

                // Scale MULTIPLIES the authored size, the same rule a sprite's extent follows. X
                // only: a text scaled differently on the two axes would need a non-uniform glyph
                // path, and nothing asks for one.
                TextDrawParams lParams;
                lParams.Color           = InText.Color;
                lParams.Size            = InText.Size * InXf.Scale.x;
                lParams.LineHeightScale = InText.LineHeightScale;
                lParams.bKerning        = InText.bKerning;
                lParams.Layer           = InText.Layer;
                lParams.OrderInLayer    = InText.OrderInLayer;

                Text2D::DrawString(InRenderer, InText.Text.CStr(), InXf.Position, lFace, lParams);
            });
    }

    bool RendererManager::ResolveSpriteDraw(const SpriteComponent& InSprite, ITexture2D*& OutTexture, SpriteUVRect& OutUV)
    {
        // THE PRECEDENCE, in one place: a sheet wins when it is set, otherwise the texture. No image
        // named at all is a normal authoring state — a component just added, or one whose image was
        // cleared — so it draws nothing rather than a white quad that reads as a broken sprite.
        const SpriteSheetData* lSheet = ResolveSheet(InSprite.Sheet);

        if (lSheet == nullptr)
        {
            OutTexture = ResolveTexture(InSprite.Texture);
            OutUV      = SpriteUVRect{};

            return OutTexture != nullptr;
        }

        OutTexture = ResolveTexture(lSheet->Texture);
        if (OutTexture == nullptr)
        {
            return false;
        }

        const SpriteFrame* lFrame = lSheet->FrameAt(InSprite.Frame);

        if (lFrame == nullptr)
        {
            // A sheet with no frames at all is simply its whole texture, which is what an author
            // sees the moment they create one — not worth a warning. An explicit index that does
            // not exist IS worth one, ONCE per sheet: it is a typo with a plausible-looking result.
            if (InSprite.Frame >= 0 && lSheet->FrameCount() > 0)
            {
                const Uint32 lKey = OpaaxStringID(InSprite.Sheet.Path).GetId();

                if (m_WarnedFrameRange.emplace(lKey).second)
                {
                    OPAAX_LOG(LogRendererManager, Warn, "Sheet '{}' has no frame {} ({} frame(s)) — drawing the whole texture",
                              InSprite.Sheet.Path.CStr(), InSprite.Frame, lSheet->FrameCount());
                }
            }

            OutUV = SpriteUVRect{};
            return true;
        }

        OutUV = MakeFrameUV(*lFrame, OutTexture->GetWidth(), OutTexture->GetHeight());

        return true;
    }

    const SpriteSheetData* RendererManager::ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath)
    {
        if (InPath.IsEmpty())
        {
            return nullptr;
        }

        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_SheetCache.find(lKey.GetId());
        if (lIt == m_SheetCache.end())
        {
            const OpaaxString lAbsolute = OpaaxApplication::GetAppService<IPaths>().AssetToAbsolute(InPath.Path);

            ResourceRef<SpriteSheetResource> lRef =
                OpaaxApplication::GetAppService<IEngine>().GetResources().Load<SpriteSheetResource>(lAbsolute.CStr());

            // Cached even when the load FAILED, for ResolveTexture's reason: keeping the empty ref
            // stops a missing file being retried once per sprite per frame.
            lIt = m_SheetCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (const SpriteSheetResource* lLoaded = lIt->second.IsValid() ? lIt->second.Get() : nullptr)
            {
                OPAAX_LOG(LogRendererManager, Info, "Sheet '{}' -> {} frame(s) of '{}'",
                          InPath.Path.CStr(), lLoaded->Data.FrameCount(), lLoaded->Data.Texture.Path.CStr());
            }
            else
            {
                OPAAX_LOG(LogRendererManager, Warn, "Sheet '{}' did not load — the sprite draws nothing",
                          InPath.Path.CStr());
            }
        }

        const SpriteSheetResource* lResource = lIt->second.Get();

        return (lResource != nullptr) ? &lResource->Data : nullptr;
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

    FontFaceView RendererManager::ResolveFace(const TResourcePath<FontFaceResource>& InPath)
    {
        if (InPath.IsEmpty())
        {
            return FontFaceView{};
        }

        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_FaceCache.find(lKey.GetId());
        if (lIt == m_FaceCache.end())
        {
            const OpaaxString lAbsolute = OpaaxApplication::GetAppService<IPaths>().AssetToAbsolute(InPath.Path);

            ResourceRef<FontFaceResource> lRef =
                OpaaxApplication::GetAppService<IEngine>().GetResources().Load<FontFaceResource>(lAbsolute.CStr());

            // Cached even when the load FAILED, for ResolveTexture's reason: keeping the empty ref
            // stops a missing file being retried once per text per frame. The failed ref resolves to
            // the empty placeholder face, which draws tofu.
            lIt = m_FaceCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (const FontFaceResource* lLoaded = lIt->second.IsValid() ? lIt->second.Get() : nullptr)
            {
                OPAAX_LOG(LogRendererManager, Info, "Face '{}' -> {} glyph(s), atlas {}x{}",
                          InPath.Path.CStr(), lLoaded->Face.GlyphCount(),
                          lLoaded->Face.AtlasWidth, lLoaded->Face.AtlasHeight);
            }
            else
            {
                OPAAX_LOG(LogRendererManager, Warn, "Face '{}' did not load — the text draws tofu",
                          InPath.Path.CStr());
            }
        }

        FontFaceResource* lResource = lIt->second.Get();
        if (lResource == nullptr)
        {
            return FontFaceView{};
        }

        // The atlas is null while the upload is still in flight. The view stays VALID: the walker
        // lays the line out and draws none of it, so the frame after lands in the right place.
        return FontFaceView{ &lResource->Face, lResource->GetAtlas() };
    }

    const FontFamilyData* RendererManager::ResolveFamily(const TResourcePath<FontFamilyResource>& InPath)
    {
        if (InPath.IsEmpty())
        {
            return nullptr;
        }

        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_FamilyCache.find(lKey.GetId());
        if (lIt == m_FamilyCache.end())
        {
            const OpaaxString lAbsolute = OpaaxApplication::GetAppService<IPaths>().AssetToAbsolute(InPath.Path);

            ResourceRef<FontFamilyResource> lRef =
                OpaaxApplication::GetAppService<IEngine>().GetResources().Load<FontFamilyResource>(lAbsolute.CStr());

            lIt = m_FamilyCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (const FontFamilyResource* lLoaded = lIt->second.IsValid() ? lIt->second.Get() : nullptr)
            {
                OPAAX_LOG(LogRendererManager, Info, "Family '{}' -> {} face(s)",
                          InPath.Path.CStr(), lLoaded->Data.EntryCount());
            }
            else
            {
                OPAAX_LOG(LogRendererManager, Warn, "Family '{}' did not load — the text draws nothing",
                          InPath.Path.CStr());
            }
        }

        const FontFamilyResource* lResource = lIt->second.Get();

        return (lResource != nullptr) ? &lResource->Data : nullptr;
    }

    FontFaceView RendererManager::ResolveTextDraw(const TextComponent& InText)
    {
        // THE PRECEDENCE, in one place: a family wins when it is set, otherwise the face named
        // directly. Naming neither is a normal authoring state — a component just added — so it
        // draws nothing rather than a row of boxes that reads as a broken font.
        const FontFamilyData* lFamily = ResolveFamily(InText.Font);

        if (lFamily == nullptr)
        {
            return ResolveFace(InText.Face);
        }

        const FontFamilyEntry* lEntry = lFamily->Find(InText.Style);

        // ONCE per (family, style), not per frame: this runs inside the draw loop, and the same
        // sentence sixty times a second is noise rather than a diagnostic.
        const Uint64 lWarnKey = (static_cast<Uint64>(OpaaxStringID(InText.Font.Path).GetId()) << 32)
                              |  static_cast<Uint64>(PackStyle(InText.Style));

        if (lEntry == nullptr)
        {
            if (m_WarnedFontStyle.emplace(lWarnKey).second)
            {
                OPAAX_LOG(LogRendererManager, Warn, "Family '{}' has no {} face — the text draws tofu",
                          InText.Font.Path.CStr(), ToString(InText.Style.Subset));
            }

            return FontFaceView{ &TofuFace(), nullptr };
        }

        if (lEntry->Style != InText.Style && m_WarnedFontStyle.emplace(lWarnKey).second)
        {
            // A fallback is never silent. The subset always matches — Find refuses to cross it — so
            // what differed is one of the other three, and naming both cuts says which.
            OPAAX_LOG(LogRendererManager, Warn,
                      "Family '{}' has no {}/{}/{} face — drawing {}/{}/{} instead",
                      InText.Font.Path.CStr(),
                      ToString(InText.Style.Width), ToString(InText.Style.Slant), ToString(InText.Style.Weight),
                      ToString(lEntry->Style.Width), ToString(lEntry->Style.Slant), ToString(lEntry->Style.Weight));
        }

        return ResolveFace(lEntry->Face);
    }

    void RendererManager::SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays)
    {
        m_SubmittedViews.emplace_back(RenderPassRequest{ &InTarget, InView, bInDrawOverlays });
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
