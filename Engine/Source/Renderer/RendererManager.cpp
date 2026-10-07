#include "Renderer/RendererManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IWindowManager.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/IEngine.h"
#include "Core/Profiling/Profiler.h"   // OPAAX_STAT_SCOPE

#include "Renderer/Config/Config_Renderer.h"

#include "Core/Events/EventBus.h"
#include "Platform/Window/WindowEvents.h"

#include "RHI/RHIBackend.h"
#include "RHI/IGraphicsContext.h"
#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"

#include "Renderer/CameraView.h"
#include "Renderer/RenderSystem.h"
#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"
#include "UI/UICanvas.h"

#include "Renderer/ShaderSource.h"
#include "Core/IO/FileIO.h"

#include "World/WorldManager.h"
#include "World/World.h"
#include "Renderer/Components/EnvironmentComponent.h"
#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/Components/ShadowCaster2DComponent.h"
#include "Renderer/Lighting/Lighting2D.h"
#include "Resources/DataAsset/DataAssetHandle.h"
#include "Renderer/Components/QuadComponent.h"
#include "Renderer/Post/ScenePipeline2D.h"
#include "Renderer/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // ComposeChain

#include "Core/Maths/Maths.h"

#include "Resources/ResourceManager.h"
#include "Renderer/Textures/TextureResource.h"
#include "Renderer/Textures/SpriteSheetResource.h"
#include "Renderer/Text/FontFaceResource.h"
#include "Renderer/Text/FontFamilyResource.h"

#include "Renderer/Text/Text2D.h"
#include "Renderer/Components/TextComponent.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/Subsystems/EngineEventBus.h"

namespace Opaax
{
    namespace
    {
        /**
         * The four style axes packed in one integer (one byte each), used as a warning key.
         */
        Uint32 PackStyle(const FontStyleKey& InKey) noexcept
        {
            return (static_cast<Uint32>(InKey.Subset)              << 24)
                 | ((WeightValue(InKey.Weight) / 100u)             << 16)
                 | (static_cast<Uint32>(InKey.Width)               << 8)
                 |  static_cast<Uint32>(InKey.Slant);
        }

        /**
         * The face used when a family has nothing for the requested script: draws boxes,
         * which makes the problem obvious.
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
        // Resolve host state and pack it into a plain desc for the render module.
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

        // The host reads the shader file; ShaderSource only parses and compiles the text.
        const OpaaxString lShaderPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Sprite.glsl");

        const OpaaxString lShaderSrc = FileIO::ReadAllText(lShaderPath);
        if (lShaderSrc.IsEmpty())
        {
            OPAAX_LOG(LogRendererManager, Error, "cannot read shader file '{}'", lShaderPath.CStr());
        }

        // The HDR composite of worlds with an Environment. Without it they are drawn directly (logged).
        const OpaaxString lTonemapPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Tonemap.glsl");
        const OpaaxString lTonemapSrc = FileIO::ReadAllText(lTonemapPath);

        // The shadow map pass. Without it, lights cast no shadows (logged).
        const OpaaxString lShadowPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Shadow2D.glsl");
        const OpaaxString lShadowSrc = FileIO::ReadAllText(lShadowPath);

        RenderSystemDesc lDesc;
        lDesc.TonemapShader = lTonemapSrc.IsEmpty() ? ShaderDesc{} : ShaderSource::FromSource(lTonemapSrc, lTonemapPath);
        lDesc.ShadowShader  = lShadowSrc.IsEmpty() ? ShaderDesc{} : ShaderSource::FromSource(lShadowSrc, lShadowPath);
        lDesc.Backend      = ResolveSupportedBackend(lEngineCfg.Render.Backend);
        lDesc.Surface      = lSurface;
        lDesc.Width        = lWindow->GetWidth();
        lDesc.Height       = lWindow->GetHeight();
        lDesc.SpriteShader = ShaderSource::FromSource(lShaderSrc, lShaderPath);
        lDesc.ClearColor   = lRenderCfg.ClearColor;

        m_bInterpolate     = lEngineCfg.Render.bInterpolation;

        lDesc.Limits.MaxQuads        = lRenderCfg.MaxQuadsPerBatch;
        lDesc.Limits.MaxTextureSlots = lRenderCfg.MaxTextureSlots;

        m_RenderSystem = MakeUnique<RenderSystem>();
        if (!m_RenderSystem->Init(lDesc))
        {
            OPAAX_LOG(LogRendererManager, Error, "RenderSystem failed to initialize");
            m_RenderSystem.reset();
            return false;
        }

        // Window resize comes through the event bus.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus()
            .Subscribe<WindowResize>(this, &RendererManager::HandleWindowResize);

        // Apply live config changes when notified.
        m_RendererConfig = &lConfigSys.Get<Config_Renderer>();
        m_RendererConfig->OnChanged().AddMember(this, &RendererManager::HandleRendererConfigChanged);

        m_EngineConfig = &lConfigSys.Get<Config_Engine>();
        m_EngineConfig->OnChanged().AddMember(this, &RendererManager::HandleEngineConfigChanged);

        // The active world is the one drawn.
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();

        return true;
    }

    void RendererManager::Shutdown()
    {
        // Unsubscribe before teardown: a late resize must not reach a destroyed m_RenderSystem.
        OpaaxApplication::GetAppService<IEngine>().GetEngineEventBus().GetEventBus().UnsubscribeAll(this);

        // Same for the config, which outlives us.
        if (m_RendererConfig != nullptr)
        {
            m_RendererConfig->OnChanged().RemoveAll(this);
            m_RendererConfig = nullptr;
        }

        if (m_EngineConfig != nullptr)
        {
            m_EngineConfig->OnChanged().RemoveAll(this);
            m_EngineConfig = nullptr;
        }

        // Release the resources first, while the ResourceManager and the GL context are alive.
        m_TextureCache.clear();
        m_SheetCache.clear();
        m_FaceCache.clear();     // holds the font atlases
        m_FamilyCache.clear();
        m_MaterialCache.clear();

        m_RenderSystem.reset(); // waits idle, then tears down
    }
    
    // =========================================================================
    // Render — the frame, then clear the per-frame queues.
    //   The queues (debug draw, submitted views) are cleared even when nothing
    //   could be rendered, so they never grow.
    // =========================================================================
    void RendererManager::Render(double InAlpha)
    {
        // Fraction of a fixed step past the last one, for display interpolation only.
        m_FrameAlpha = static_cast<float>(InAlpha);

        {
            // Around RenderFrame only: the clears below must run even if nothing rendered.
            OPAAX_STAT_SCOPE("Renderer");
            RenderFrame();
        }

        // Log once when something was actually interpolated.
        if (!m_bLoggedFirstBlend && m_BlendedThisFrame > 0)
        {
            m_bLoggedFirstBlend = true;
            OPAAX_LOG(LogRendererManager, Info, "Interpolating {} drawn pose(s) — alpha {:.2f}",
                      m_BlendedThisFrame, m_FrameAlpha);
        }

        m_BlendedThisFrame = 0;

        SubmitRenderCounters();

        m_DebugDraw.Clear();
        m_SubmittedViews.clear();
        m_SubmittedCanvases.clear();
    }

    DisplayPose RendererManager::PoseFor(World& InWorld, const EntityID InEntity,
                                         const TransformComponent& InTransform)
    {
        // Each parent's local pose is blended, then the chain is composed, so a child of an
        // interpolated body is drawn with it.
        const auto lDisplayLocal = [this, &InWorld, InEntity, &InTransform](Entity InHop) -> TransformComponent
        {
            const TransformComponent* lLocal = InHop.GetHandle() == InEntity ? &InTransform
                                                                             : InHop.TryGet<TransformComponent>();
            if (lLocal == nullptr) { return TransformComponent{}; }

            // Off means no blend (alpha 0 would be the previous pose, one step behind).
            if (!m_bInterpolate) { return *lLocal; }

            const auto* lPrevious = InWorld.GetRegistry().try_get<TransformInterpolationComponent>(InHop.GetHandle());

            const DisplayPose lBlend = ResolveDisplayPose(*lLocal, lPrevious, m_FrameAlpha);

            // Counted only when the blend actually moved the entity.
            if (InHop.GetHandle() == InEntity && lPrevious != nullptr && lPrevious->bHasPrevious
                && (lBlend.Position != lLocal->Position || lBlend.RotationDeg != lLocal->Rotation))
            {
                ++m_BlendedThisFrame;
            }

            TransformComponent lOut = *lLocal;
            lOut.Position = lBlend.Position;
            lOut.Rotation = lBlend.RotationDeg;
            return lOut;
        };

        const TransformComponent lWorld = EntityHierarchy::ComposeChain(Entity{ InEntity, &InWorld }, lDisplayLocal);

        return DisplayPose{ lWorld.Position, lWorld.Rotation, lWorld.Scale };
    }

    void RendererManager::SubmitRenderCounters()
    {
        Profiler& lProfiler = Profiler::Get();
        if (!lProfiler.IsEnabled()) { return; }

        lProfiler.SubmitGpuMs(
            m_RenderSystem ? m_RenderSystem->GetGpuFrameTimeMs() : -1.0);

        // Outside RenderFrame, so a frame that drew nothing reports zeros.
        const Renderer2DStats lStats = m_RenderSystem
                                           ? m_RenderSystem->GetRenderer2D().GetStats()
                                           : Renderer2DStats{};

        // Published as named counters, so Core and the Stats panel know nothing of the renderer.
        lProfiler.AddCount("Draw Calls",    lStats.DrawCalls);
        lProfiler.AddCount("Quads",         lStats.Quads);
        lProfiler.AddCount("Texture Slots", lStats.PeakTextureSlots);

        // UI cost (0 / 0 on an idle frame).
        lProfiler.AddCount("UI Layouts",  m_UILayouts);
        lProfiler.AddCount("UI Rebuilds", m_UIRebuilds);
        m_UILayouts  = 0;
        m_UIRebuilds = 0;
    }

    // =========================================================================
    // RenderFrame — one device frame, one pass per view.
    //   With nothing submitted, the active world is drawn to the backbuffer.
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
            // No camera view for this world: use the default CameraView.
            SubmitRenderView(m_RenderSystem->GetBackbuffer(),
                             lWorld != nullptr ? lWorld->GetCameraView() : CameraView{},
                             /*bInDrawOverlays*/ true, /*InSource*/ nullptr, /*bInDrawUI*/ true);
        }

        // Counted before opening the device frame, so a frame with nothing to draw opens none.
        Uint32 lPasses = 0;

        for (const RenderPassRequest& lRequest : m_SubmittedViews)
        {
            if (lRequest.Target != nullptr && lRequest.Target->GetWidth() > 0 && lRequest.Target->GetHeight() > 0)
            {
                ++lPasses;
            }
        }

        // A canvas with its own target draws even without a viewport.
        for (const UICanvasRequest& lCanvasRequest : m_SubmittedCanvases)
        {
            if (lCanvasRequest.Target != nullptr && lCanvasRequest.Target->GetWidth() > 0
                && lCanvasRequest.Target->GetHeight() > 0)
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

            // The view's own world if it named one, else the active world.
            RenderPass(*lRequest.Target, lRequest.Source != nullptr ? lRequest.Source : lWorld,
                       lRequest.View, lRequest.bDrawOverlays);

            if (lRequest.bDrawUI && !m_SubmittedCanvases.empty())
            {
                RenderCanvases(*lRequest.Target);
            }
        }

        // A canvas with its own target gets its own pass, with no world (editor preview). Cleared.
        for (const UICanvasRequest& lCanvasRequest : m_SubmittedCanvases)
        {
            if (lCanvasRequest.Target != nullptr)
            {
                RenderCanvasPass(*lCanvasRequest.Canvas, *lCanvasRequest.Target, ELoadOp::Clear,
                                 lCanvasRequest.bHasView ? &lCanvasRequest.View : nullptr);
            }
        }

        m_RenderSystem->EndFrame();
    }

    void RendererManager::RenderPass(IRenderTarget& InTarget, World* InWorld, const CameraView& InView, bool bInDrawOverlays)
    {
        // The target's size drives the view, so a resized viewport rescales the render.
        const Uint32 lWidth  = InTarget.GetWidth();
        const Uint32 lHeight = InTarget.GetHeight();

        // The submitter gives the view; the matrix is built here, where the pixels are known.
        RenderView lView;
        lView.ViewProjection = MakeViewProjection(InView, lWidth, lHeight);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        Renderer2D& lRenderer = m_RenderSystem->GetRenderer2D();

        // A world with an Environment is drawn in HDR (linear colour), then composited into the target.
        const EnvironmentComponent* lEnvironment = (InWorld != nullptr) ? FindEnvironment(*InWorld) : nullptr;
        ScenePipeline2D&            lPipeline    = m_RenderSystem->GetScenePipeline();
        ICommandBuffer*             lCommands    = m_RenderSystem->GetCommandBuffer();

        if (lEnvironment != nullptr && lPipeline.IsReady() && lCommands != nullptr)
        {
            RenderView lSceneView   = lView;
            lSceneView.bLinearColor = true;

            // The clear colour is authored in screen colour too.
            const Vector4F& lClear = m_RenderSystem->GetClearColor();
            const Vector4F  lLinearClear{ Maths::Pow(lClear.r, 2.2f), Maths::Pow(lClear.g, 2.2f),
                                          Maths::Pow(lClear.b, 2.2f), lClear.a };

            // The lights that reach this view.
            const float    lHalfHeight = InView.OrthoSize;
            const float    lHalfWidth  = lHalfHeight * (static_cast<float>(lWidth) / static_cast<float>(std::max(lHeight, 1u)));
            const Bounds2D lViewBounds{ InView.Position, { lHalfWidth, lHalfHeight } };
            const Vector3F lAmbient{ lEnvironment->AmbientColor.r, lEnvironment->AmbientColor.g, lEnvironment->AmbientColor.b };

            LightsBlock2D lLights;
            PackLights2D(CollectLights(*InWorld), lViewBounds, lAmbient, lEnvironment->AmbientIntensity, lLights);

            // Shadows first: they are drawn from passes of their own.
            const float lPixelsPerUnit = static_cast<float>(lHeight) / std::max(2.f * InView.OrthoSize, 1e-3f);
            ITexture2D* lShadowMap     = RenderShadowMap(*InWorld, lRenderer, lViewBounds, lPixelsPerUnit, lLights);
            if (lShadowMap == nullptr)
            {
                ClearShadowRows2D(lLights);
            }

            lRenderer.SetLighting(lLights);
            lRenderer.SetShadowMap(lShadowMap);

            // Overlays meant to be behind the world (the grid) go in with it.
            m_RenderSystem->BeginPass(lPipeline.PrepareScene(lWidth, lHeight), lSceneView, ELoadOp::Clear, &lLinearClear);
            DrawWorld(*InWorld, lRenderer);
            if (bInDrawOverlays) { DrawOverlays(InWorld, lRenderer, EOverlayFilter::BehindWorld); }
            m_RenderSystem->EndPass();

            lRenderer.SetShadowMap(nullptr);

            lPipeline.Composite(*lCommands, InTarget, PostSettings::From(*lEnvironment));

            // The other overlays over the finished picture, in screen colour.
            if (bInDrawOverlays)
            {
                m_RenderSystem->BeginPass(InTarget, lView, ELoadOp::Load);
                DrawOverlays(InWorld, lRenderer, EOverlayFilter::OverWorld);
                m_RenderSystem->EndPass();
            }
            return;
        }

        // Lights need the HDR path: said once, since a level without an Environment ignores them.
        if (InWorld != nullptr && !m_bWarnedLightsIgnored && lEnvironment == nullptr)
        {
            bool bHasLight = false;
            InWorld->Each<Light2DComponent>([&bHasLight](EntityID, const Light2DComponent& InLight)
            {
                bHasLight = bHasLight || InLight.bEnabled;
            });

            if (bHasLight)
            {
                m_bWarnedLightsIgnored = true;
                OPAAX_LOG(LogRendererManager, Warn,
                          "World '{}' has Light2Ds but no EnvironmentComponent: they light nothing until one is added",
                          InWorld->GetName().CStr());
            }
        }

        m_RenderSystem->BeginPass(InTarget, lView);

        if (InWorld != nullptr)
        {
            DrawWorld(*InWorld, lRenderer);
        }

        if (bInDrawOverlays)
        {
            DrawOverlays(InWorld, lRenderer, EOverlayFilter::All);
        }

        m_RenderSystem->EndPass();
    }

    TDynArray<Light2DInstance> RendererManager::CollectLights(World& InWorld)
    {
        TDynArray<Light2DInstance> lLights;
        InWorld.Each<TransformComponent, Light2DComponent>(
            [this, &InWorld, &lLights](EntityID InEntity, TransformComponent& InXf, Light2DComponent& InLight)
            {
                // Where the entity is drawn this frame, so a light carried by a sprite stays on it.
                const DisplayPose lPose = PoseFor(InWorld, InEntity, InXf);
                lLights.push_back(Light2DInstance{ &InLight, lPose.Position, lPose.RotationDeg });
            });
        return lLights;
    }

    QuadLighting RendererManager::ResolveLighting(const TDataAssetRef<Material2D>& InMaterial)
    {
        QuadLighting lLighting;
        if (InMaterial.IsEmpty())
        {
            return lLighting;
        }

        const OpaaxStringID lKey(InMaterial.Path);
        auto lIt = m_MaterialCache.find(lKey.GetId());
        if (lIt == m_MaterialCache.end())
        {
            // Kept even when it fails to load, so a missing file is not retried every frame.
            lIt = m_MaterialCache.emplace(lKey.GetId(),
                                          LoadDataAsset(OpaaxApplication::GetAppService<IEngine>().GetResources(),
                                                        OpaaxApplication::GetAppService<IPaths>(), InMaterial)).first;
        }

        const Material2D* lMaterial = lIt->second.Get();
        if (lMaterial == nullptr)
        {
            return lLighting;
        }

        lLighting.bLit      = lMaterial->bLit;
        lLighting.NormalMap = ResolveTexture(lMaterial->NormalMap);

        const float lStrength = std::max(lMaterial->EmissiveStrength, 0.f);
        lLighting.Emissive = Vector3F{ ScreenToLinear(lMaterial->EmissiveColor.r) * lStrength,
                                       ScreenToLinear(lMaterial->EmissiveColor.g) * lStrength,
                                       ScreenToLinear(lMaterial->EmissiveColor.b) * lStrength };
        return lLighting;
    }

    ITexture2D* RendererManager::RenderShadowMap(World& InWorld, Renderer2D& InRenderer, const Bounds2D& InView,
                                                 const float InPixelsPerUnit, const LightsBlock2D& InLights)
    {
        ScenePipeline2D& lPipeline = m_RenderSystem->GetScenePipeline();
        ICommandBuffer*  lCommands = m_RenderSystem->GetCommandBuffer();

        ShadowBlock2D lShadows;
        if (!lPipeline.CanShadow() || lCommands == nullptr || BuildShadowBlock2D(InLights, lShadows) == 0)
        {
            return nullptr;
        }

        bool bHasCaster = false;
        InWorld.Each<ShadowCaster2DComponent>([&bHasCaster](EntityID, const ShadowCaster2DComponent& InCaster)
        {
            bHasCaster = bHasCaster || InCaster.bEnabled;
        });

        if (!bHasCaster)
        {
            return nullptr;
        }

        const OcclusionLayout2D lLayout = MakeOcclusionLayout2D(InView, InPixelsPerUnit, lShadows);
        const Vector2F          lMin    = lLayout.Bounds.Min();
        const Vector2F          lSize   = lLayout.Bounds.Size();
        lShadows.OcclusionRect = Vector4F{ lMin.x, lMin.y, lSize.x, lSize.y };

        // The casters' coverage over the layout's area: its bounds match its pixels exactly.
        RenderView lView;
        lView.ViewProjection = MakeViewProjection(CameraView{ lLayout.Bounds.Center, lLayout.Bounds.HalfExtent.y },
                                                  lLayout.Width, lLayout.Height);
        lView.Viewport       = Viewport{ 0, 0, lLayout.Width, lLayout.Height };
        lView.bOcclusion     = true;

        const Vector4F lNothing{ 0.f, 0.f, 0.f, 0.f };
        m_RenderSystem->BeginPass(lPipeline.PrepareOcclusion(lLayout.Width, lLayout.Height), lView, ELoadOp::Clear, &lNothing);
        const Uint32 lCasters = DrawShadowCasters(InWorld, InRenderer);
        m_RenderSystem->EndPass();

        // Every caster hidden: nothing can cast.
        return (lCasters > 0) ? lPipeline.BuildShadowMap(*lCommands, lShadows) : nullptr;
    }

    Uint32 RendererManager::DrawShadowCasters(World& InWorld, Renderer2D& InRenderer)
    {
        Uint32 lDrawn = 0;
        InWorld.Each<TransformComponent, ShadowCaster2DComponent>(
            [this, &InRenderer, &InWorld, &lDrawn](EntityID InEntity, TransformComponent& InXf, ShadowCaster2DComponent& InCaster)
            {
                if (!InCaster.bEnabled)
                {
                    return;
                }

                Entity            lEntity(InEntity, &InWorld);
                const DisplayPose lPose     = PoseFor(InWorld, InEntity, InXf);
                const float       lRotation = Maths::DegreesToRadians(lPose.RotationDeg);

                // Its silhouette: the sprite's alpha, or the whole quad.
                if (const SpriteComponent* lSprite = lEntity.TryGet<SpriteComponent>())
                {
                    ITexture2D*  lTexture = nullptr;
                    SpriteUVRect lUV;
                    if (lSprite->bVisible && ResolveSpriteDraw(*lSprite, lTexture, lUV))
                    {
                        InRenderer.DrawSprite(lPose.Position, lSprite->Size * lPose.Scale, *lTexture, lSprite->Color,
                                              lRotation, lSprite->Layer, lSprite->OrderInLayer, lUV.UVMin, lUV.UVMax);
                        ++lDrawn;
                    }
                }
                else if (const QuadComponent* lQuad = lEntity.TryGet<QuadComponent>())
                {
                    InRenderer.DrawQuad(lPose.Position, lQuad->Size * lPose.Scale, lQuad->Color, lRotation);
                    ++lDrawn;
                }
            });
        return lDrawn;
    }

    bool RendererManager::ReceivesShadows(World& InWorld, const EntityID InEntity)
    {
        const ShadowCaster2DComponent* lCaster = Entity(InEntity, &InWorld).TryGet<ShadowCaster2DComponent>();
        return lCaster == nullptr || !lCaster->bEnabled || lCaster->bSelfShadows;
    }

    const EnvironmentComponent* RendererManager::FindEnvironment(World& InWorld)
    {
        const EnvironmentComponent* lFound = nullptr;
        InWorld.Each<EnvironmentComponent>([&lFound](EntityID, const EnvironmentComponent& InEnvironment)
        {
            if (lFound == nullptr) { lFound = &InEnvironment; }
        });
        return lFound;
    }

    void RendererManager::DrawWorld(World& InWorld, Renderer2D& InRenderer)
    {
        // A solid quad per QuadComponent, a textured one per Sprite, glyphs per Text.
        InWorld.Each<TransformComponent, QuadComponent>(
            [this, &InRenderer, &InWorld](EntityID InEntity, TransformComponent& InXf, QuadComponent& InComp)
            {
                const DisplayPose lPose = PoseFor(InWorld, InEntity, InXf);

                QuadLighting lLighting;
                lLighting.bReceiveShadows = ReceivesShadows(InWorld, InEntity);

                // Scale multiplies the component's Size.
                InRenderer.DrawQuad(lPose.Position, InComp.Size * lPose.Scale, InComp.Color,
                                    Maths::DegreesToRadians(lPose.RotationDeg), ERenderLayer::Default, 0,
                                    QuadMask{}, lLighting);
            });

        DrawWorldSprites(InWorld, InRenderer);
        DrawWorldTexts(InWorld, InRenderer);
    }

    void RendererManager::DrawOverlays(World* InWorld, Renderer2D& InRenderer, const EOverlayFilter InFilter)
    {
        // Read per pass, cleared once per frame. A primitive without a world belongs to the active
        // world; other worlds only draw what was queued for them.
        const World* const lActive = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;
        const auto lBelongsHere = [InWorld, lActive](const World* InSource)
        {
            return (InSource != nullptr ? InSource : lActive) == InWorld;
        };

        const auto lPasses = [InFilter](const ERenderLayer InLayer)
        {
            const bool bBehind = InLayer < ERenderLayer::Default;
            return InFilter == EOverlayFilter::All || (InFilter == EOverlayFilter::BehindWorld) == bBehind;
        };

        // Helpers keep their colour whatever the level's lighting.
        QuadLighting lUnlit;
        lUnlit.bLit = false;

        // Each line is a thin rotated quad, on its own layer (the grid is behind world geometry).
        for (const DebugLine& lLine : m_DebugDraw.GetLines())
        {
            if (!lBelongsHere(lLine.Source) || !lPasses(lLine.Layer)) { continue; }

            const DebugQuad lQuad = ToQuad(lLine);
            InRenderer.DrawQuad(lQuad.Center, lQuad.Size, lLine.Color, lQuad.RotationRad, lLine.Layer, 0,
                                QuadMask{}, lUnlit);
        }

        // Boxes are one hollow quad each, same layer rule.
        for (const DebugBox& lBox : m_DebugDraw.GetBoxes())
        {
            if (!lBelongsHere(lBox.Source) || !lPasses(lBox.Layer)) { continue; }

            InRenderer.DrawQuadOutline(lBox.Center, lBox.Size, lBox.Color, lBox.Thickness,
                                       lBox.RotationRad, lBox.Layer, 0, QuadMask{}, lUnlit);
        }
    }

    void RendererManager::RenderCanvases(IRenderTarget& InTarget)
    {
        const Uint32 lWidth  = InTarget.GetWidth();
        const Uint32 lHeight = InTarget.GetHeight();

        for (const UICanvasRequest& lRequest : m_SubmittedCanvases)
        {
            // A canvas with its own target is drawn only there, and world canvases are not drawn into it.
            if (lRequest.Target != nullptr) { continue; }

            // Load, not Clear: the canvas goes over the world.
            RenderCanvasPass(*lRequest.Canvas, InTarget, ELoadOp::Load);
        }

        if (!m_bLoggedFirstUI)
        {
            m_bLoggedFirstUI = true;
            OPAAX_LOG(LogRendererManager, Info, "First UI frame: {} canvas(es) over a {}x{} target, {} layout(s)",
                      static_cast<Uint64>(m_SubmittedCanvases.size()), lWidth, lHeight, m_UILayouts);
        }
    }

    void RendererManager::RenderCanvasPass(UICanvas& InCanvas, IRenderTarget& InTarget, const ELoadOp InLoadOp,
                                           const CameraView* InView)
    {
        const Uint32 lWidth  = InTarget.GetWidth();
        const Uint32 lHeight = InTarget.GetHeight();

        if (lWidth == 0 || lHeight == 0) { return; }

        // A canvas with a hidden root opens no pass, so a loading screen costs nothing when hidden.
        if (!InCanvas.Root().bVisible) { return; }

        // The target size drives the layout, unless the submitter gave its own view
        // (then it already laid the canvas out).
        if (InView == nullptr)
        {
            InCanvas.SetTargetSize(lWidth, lHeight);
        }

        const UIBuildContext lContext{ this };
        const UICanvasStats  lStats = InCanvas.Update(lContext);
        m_UILayouts  += lStats.Layouts;
        m_UIRebuilds += lStats.Rebuilds;

        RenderView lView;
        lView.ViewProjection = MakeViewProjection(InView != nullptr ? *InView : InCanvas.MakeView(), lWidth, lHeight);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginPass(InTarget, lView, InLoadOp);
        InCanvas.Submit(m_RenderSystem->GetRenderer2D());
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
            [this, &InRenderer, &InWorld](EntityID InEntity, TransformComponent& InXf, SpriteComponent& InSprite)
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

                const DisplayPose lPose = PoseFor(InWorld, InEntity, InXf);

                QuadLighting lLighting    = ResolveLighting(InSprite.Material);
                lLighting.bReceiveShadows = ReceivesShadows(InWorld, InEntity);

                InRenderer.DrawSprite(lPose.Position, InSprite.Size * lPose.Scale, *lTexture, InSprite.Color,
                                      Maths::DegreesToRadians(lPose.RotationDeg),
                                      InSprite.Layer, InSprite.OrderInLayer,
                                      lUV.UVMin, lUV.UVMax, QuadMask{}, lLighting);
            });
    }

    void RendererManager::DrawWorldTexts(World& InWorld, Renderer2D& InRenderer)
    {
        InWorld.Each<TransformComponent, TextComponent>(
            [this, &InRenderer, &InWorld](EntityID InEntity, TransformComponent& InXf, TextComponent& InText)
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

                const DisplayPose lPose = PoseFor(InWorld, InEntity, InXf);

                // Scale multiplies the authored size (X only).
                TextDrawParams lParams;
                lParams.Color           = InText.Color;
                lParams.Size            = InText.Size * lPose.Scale.x;
                lParams.LineHeightScale = InText.LineHeightScale;
                lParams.bKerning        = InText.bKerning;
                lParams.Layer           = InText.Layer;
                lParams.OrderInLayer    = InText.OrderInLayer;

                Text2D::DrawString(InRenderer, InText.Text.CStr(), lPose.Position, lFace, lParams);
            });
    }

    bool RendererManager::ResolveSpriteDraw(const SpriteComponent& InSprite, ITexture2D*& OutTexture, SpriteUVRect& OutUV)
    {
        // A sheet wins over a texture. No image at all draws nothing.
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
            // A sheet with no frames is its whole texture. A missing frame index warns once.
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

            // Cached even when loading failed, so a missing file is not retried every frame.
            lIt = m_SheetCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (!lIt->second.IsValid() || lIt->second.Get() == nullptr)
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

            // Cached even when loading failed (magenta placeholder), so it is not retried every frame.
            lIt = m_TextureCache.emplace(lKey.GetId(), Move(lRef)).first;

            // Logged once per texture, success or failure (on separate lines).
            if (const TextureResource* lLoaded = lIt->second.IsValid() ? lIt->second.Get() : nullptr)
            {
                OPAAX_LOG(LogRendererManager, Trace, "Texture '{}' -> {}x{}",
                          InPath.Path.CStr(), lLoaded->Width, lLoaded->Height);
            }
            else
            {
                OPAAX_LOG(LogRendererManager, Warn, "Texture '{}' did not load — drawing the placeholder",
                          InPath.Path.CStr());
            }
        }

        TextureResource* lResource = lIt->second.Get();

        // Null while loading or without a device: not drawable this frame. No log.
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

            // Cached even when loading failed (empty placeholder face, draws boxes).
            lIt = m_FaceCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (!lIt->second.IsValid() || lIt->second.Get() == nullptr)
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

        // The atlas is null while uploading; the layout is still done, nothing is drawn.
        return FontFaceView{ &lResource->Face, lResource->GetAtlas() };
    }

    FontFaceView RendererManager::ResolveFace(const char* InAssetPath)
    {
        TResourcePath<FontFaceResource> lPath;
        lPath.Path = OpaaxString(InAssetPath);
        return ResolveFace(lPath);
    }

    ITexture2D* RendererManager::ResolveTexture(const char* InAssetPath)
    {
        TResourcePath<TextureResource> lPath;
        lPath.Path = OpaaxString(InAssetPath);
        return ResolveTexture(lPath);
    }

    UISheetFrameView RendererManager::ResolveSheetFrame(const char* InSheetPath, const Int32 InFrame)
    {
        TResourcePath<SpriteSheetResource> lPath;
        lPath.Path = OpaaxString(InSheetPath);

        UISheetFrameView lView;

        const SpriteSheetData* lSheet = ResolveSheet(lPath);
        if (lSheet == nullptr)
        {
            return lView;
        }

        lView.Texture = ResolveTexture(lSheet->Texture);
        if (lView.Texture == nullptr)
        {
            return lView;   // still uploading
        }

        lView.SizePx = { static_cast<float>(lView.Texture->GetWidth()), static_cast<float>(lView.Texture->GetHeight()) };

        // A missing frame is the whole texture, warned once.
        const SpriteFrame* lFrame = lSheet->FrameAt(InFrame);
        if (lFrame == nullptr)
        {
            if (InFrame >= 0 && lSheet->FrameCount() > 0 && m_WarnedFrameRange.emplace(OpaaxStringID(lPath.Path).GetId()).second)
            {
                OPAAX_LOG(LogRendererManager, Warn, "Sheet '{}' has no frame {} ({} frame(s)) — drawing the whole texture",
                          InSheetPath, InFrame, lSheet->FrameCount());
            }
            return lView;
        }

        const SpriteUVRect lUV = MakeFrameUV(*lFrame, lView.Texture->GetWidth(), lView.Texture->GetHeight());
        lView.UVMin  = lUV.UVMin;
        lView.UVMax  = lUV.UVMax;
        lView.SizePx = lFrame->Size;
        return lView;
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

            if (!lIt->second.IsValid() || lIt->second.Get() == nullptr)
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
        // A family wins over a face. Neither draws nothing.
        const FontFamilyData* lFamily = ResolveFamily(InText.Font);

        if (lFamily == nullptr)
        {
            return ResolveFace(InText.Face);
        }

        const FontFamilyEntry* lEntry = lFamily->Find(InText.Style);

        // Warn once per (family, style), not every frame.
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
            // Warn when falling back, naming both styles.
            OPAAX_LOG(LogRendererManager, Warn,
                      "Family '{}' has no {}/{}/{} face — drawing {}/{}/{} instead",
                      InText.Font.Path.CStr(),
                      ToString(InText.Style.Width), ToString(InText.Style.Slant), ToString(InText.Style.Weight),
                      ToString(lEntry->Style.Width), ToString(lEntry->Style.Slant), ToString(lEntry->Style.Weight));
        }

        return ResolveFace(lEntry->Face);
    }

    void RendererManager::SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                                           World* InSource, bool bInDrawUI)
    {
        m_SubmittedViews.emplace_back(RenderPassRequest{ &InTarget, InView, bInDrawOverlays, bInDrawUI, InSource });
    }

    void RendererManager::SubmitUICanvas(UICanvas& InCanvas, IRenderTarget* InTarget, const CameraView* InView)
    {
        UICanvasRequest lRequest{ &InCanvas, InTarget };
        if (InView != nullptr)
        {
            lRequest.View     = *InView;
            lRequest.bHasView = true;
        }
        m_SubmittedCanvases.emplace_back(lRequest);
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

    bool RendererManager::CaptureBackbuffer(TDynArray<Uint8>& OutRGBA, Uint32& OutWidth, Uint32& OutHeight) const
    {
        return m_RenderSystem != nullptr && m_RenderSystem->CaptureBackbuffer(OutRGBA, OutWidth, OutHeight);
    }

    // =========================================================================
    // Window resize (event bus): resizes the backbuffer.
    //   Offscreen targets (editor viewport) are sized by their owner.
    // =========================================================================
    void RendererManager::HandleWindowResize(const WindowResize& InResize)
    {
        if (m_RenderSystem)
        {
            m_RenderSystem->Resize(InResize.Width, InResize.Height);
        }
    }

    void RendererManager::HandleRendererConfigChanged()
    {
        if (!m_RenderSystem || m_RendererConfig == nullptr)
        {
            return;
        }

        m_RenderSystem->SetClearColor(m_RendererConfig->GetData().ClearColor);
    }

    void RendererManager::HandleEngineConfigChanged()
    {
        if (m_EngineConfig == nullptr)
        {
            return;
        }

        m_bInterpolate = m_EngineConfig->GetData().Render.bInterpolation;
    }
}
