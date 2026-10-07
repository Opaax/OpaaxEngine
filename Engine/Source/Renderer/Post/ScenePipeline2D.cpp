#include "Renderer/Post/ScenePipeline2D.h"

#include <cmath>

#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/RenderTarget.hpp"
#include "RHI/Framebuffer.h"
#include "RHI/ICommandBuffer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/Pipeline.h"
#include "RHI/Shader.h"
#include "RHI/UniformBuffer.h"

namespace Opaax
{
    namespace
    {
        /** The post-process block's binding point (PostUBO in the post shaders). */
        constexpr Uint32 POST_UBO_BINDING = 2;

        /** The shadow pass's block (ShadowUBO in Shadow2D.glsl). */
        constexpr Uint32 SHADOW_UBO_BINDING = 4;

        /** The ambient occlusion passes' block (AmbientOcclusionUBO in AmbientOcclusion2D.glsl). */
        constexpr Uint32 AMBIENT_OCCLUSION_UBO_BINDING = 5;

        /** The bloom passes' block (BloomUBO in Bloom2D.glsl). */
        constexpr Uint32 BLOOM_UBO_BINDING = 6;

        /** The modes of Bloom2D.glsl. */
        constexpr Uint32 BLOOM_PASS_FIRST_DOWN = 0;
        constexpr Uint32 BLOOM_PASS_DOWN       = 1;
        constexpr Uint32 BLOOM_PASS_UP         = 2;

        /** The modes of AmbientOcclusion2D.glsl. */
        constexpr Uint32 AO_PASS_SHRINK      = 0;
        constexpr Uint32 AO_PASS_BLUR_ACROSS = 1;
        constexpr Uint32 AO_PASS_BLUR_DOWN   = 2;

        bool HasStages(const ShaderDesc& InShader) noexcept
        {
            return !InShader.VertexSrc.IsEmpty() && !InShader.FragmentSrc.IsEmpty();
        }

        /** A fullscreen pass over a target: every pixel written (or added to, with Additive). */
        TUniquePtr<IPipeline> MakeFullscreenPipeline(IRHIDevice& InDevice, IShader& InShader, const char* InName,
                                                     const EBlendMode InBlend = EBlendMode::None)
        {
            PipelineDesc lDesc;
            lDesc.Shader    = &InShader;
            lDesc.Blend     = InBlend;
            lDesc.DebugName = InName;
            return InDevice.CreatePipeline(lDesc);
        }
    }

    TDynArray<BloomLevel2D> MakeBloomLevels2D(const Uint32 InWidth, const Uint32 InHeight)
    {
        TDynArray<BloomLevel2D> lLevels;

        BloomLevel2D lLevel{ std::max(InWidth / 2u, 1u), std::max(InHeight / 2u, 1u) };
        lLevels.push_back(lLevel);

        while (lLevels.size() < MAX_BLOOM_LEVELS_2D)
        {
            lLevel.Width  /= 2u;
            lLevel.Height /= 2u;
            if (lLevel.Width < 4u || lLevel.Height < 4u)
            {
                break;
            }
            lLevels.push_back(lLevel);
        }

        return lLevels;
    }

    ScenePipeline2D::ScenePipeline2D()  = default;
    ScenePipeline2D::~ScenePipeline2D() { Shutdown(); }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool ScenePipeline2D::Init(IRHIDevice& InDevice, const ScenePipelineShaders& InShaders)
    {
        if (!HasStages(InShaders.Tonemap))
        {
            OPAAX_LOG(LogScenePipeline2D, Error, "No tonemap shader: worlds with an Environment are drawn without HDR");
            return false;
        }

        m_Device = &InDevice;

        m_TonemapShader   = InDevice.CreateShader(InShaders.Tonemap);
        m_TonemapPipeline = MakeFullscreenPipeline(InDevice, *m_TonemapShader, "ScenePipeline2D::Tonemap");
        m_PostUBO         = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(Vector4F)), POST_UBO_BINDING);

        if (HasStages(InShaders.Shadow))
        {
            m_ShadowShader   = InDevice.CreateShader(InShaders.Shadow);
            m_ShadowPipeline = MakeFullscreenPipeline(InDevice, *m_ShadowShader, "ScenePipeline2D::Shadow");
            m_ShadowUBO      = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(ShadowBlock2D)), SHADOW_UBO_BINDING);
        }
        else
        {
            OPAAX_LOG(LogScenePipeline2D, Warn, "No shadow shader: lights cast no shadows");
        }

        if (HasStages(InShaders.AmbientOcclusion))
        {
            m_AmbientOcclusionShader   = InDevice.CreateShader(InShaders.AmbientOcclusion);
            m_AmbientOcclusionPipeline = MakeFullscreenPipeline(InDevice, *m_AmbientOcclusionShader,
                                                                "ScenePipeline2D::AmbientOcclusion");
            m_AmbientOcclusionUBO      = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(Vector4F)),
                                                                      AMBIENT_OCCLUSION_UBO_BINDING);
        }
        else
        {
            OPAAX_LOG(LogScenePipeline2D, Warn, "No ambient occlusion shader: environments get none");
        }

        if (HasStages(InShaders.Bloom))
        {
            m_BloomShader      = InDevice.CreateShader(InShaders.Bloom);
            m_BloomPipeline    = MakeFullscreenPipeline(InDevice, *m_BloomShader, "ScenePipeline2D::Bloom");
            m_BloomAddPipeline = MakeFullscreenPipeline(InDevice, *m_BloomShader, "ScenePipeline2D::BloomAdd",
                                                        EBlendMode::Additive);
            m_BloomUBO         = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(Vector4F)), BLOOM_UBO_BINDING);
        }
        else
        {
            OPAAX_LOG(LogScenePipeline2D, Warn, "No bloom shader: environments get none");
        }

        return true;
    }

    void ScenePipeline2D::Shutdown()
    {
        m_SceneTarget.reset();
        m_Scene.reset();
        m_TonemapPipeline.reset();   // before its shader
        m_TonemapShader.reset();
        m_PostUBO.reset();

        m_ShadowMapTexture.reset();
        m_ShadowMapTarget.reset();
        m_ShadowMap.reset();
        m_OcclusionTarget.reset();
        m_Occlusion.reset();
        m_ShadowPipeline.reset();
        m_ShadowShader.reset();
        m_ShadowUBO.reset();

        m_AmbientOcclusionTexture.reset();
        for (Uint32 lIndex = 0; lIndex < 2; ++lIndex)
        {
            m_AmbientOcclusionTarget[lIndex].reset();
            m_AmbientOcclusion[lIndex].reset();
        }
        m_AmbientOcclusionPipeline.reset();
        m_AmbientOcclusionShader.reset();
        m_AmbientOcclusionUBO.reset();

        m_BloomTargets.clear();
        m_BloomLevels.clear();
        m_BloomAddPipeline.reset();
        m_BloomPipeline.reset();
        m_BloomShader.reset();
        m_BloomUBO.reset();

        m_Device = nullptr;
    }

    bool ScenePipeline2D::IsReady() const noexcept
    {
        return m_Device != nullptr && m_TonemapPipeline != nullptr && m_PostUBO != nullptr;
    }

    bool ScenePipeline2D::CanShadow() const noexcept
    {
        return IsReady() && m_ShadowPipeline != nullptr && m_ShadowUBO != nullptr;
    }

    bool ScenePipeline2D::CanAmbientOcclusion() const noexcept
    {
        return IsReady() && m_AmbientOcclusionPipeline != nullptr && m_AmbientOcclusionUBO != nullptr;
    }

    bool ScenePipeline2D::CanBloom() const noexcept
    {
        return IsReady() && m_BloomPipeline != nullptr && m_BloomAddPipeline != nullptr && m_BloomUBO != nullptr;
    }

    // =========================================================================
    // Frame
    // =========================================================================
    IRenderTarget& ScenePipeline2D::PrepareScene(const Uint32 InWidth, const Uint32 InHeight)
    {
        if (m_Scene == nullptr)
        {
            FramebufferSpec lSpec;
            lSpec.Width        = InWidth;
            lSpec.Height       = InHeight;
            lSpec.DepthStencil = false;
            lSpec.ColorFormat  = ETextureFormat::RGBA16F;

            m_Scene       = m_Device->CreateFramebuffer(lSpec);
            m_SceneTarget = MakeUnique<OffscreenRenderTarget>(m_Scene.get());
        }
        else
        {
            m_Scene->Resize(InWidth, InHeight);
        }

        return *m_SceneTarget;
    }

    void ScenePipeline2D::Composite(ICommandBuffer& InCmd, IRenderTarget& InOutput, const PostSettings& InSettings)
    {
        if (!IsReady() || m_Scene == nullptr)
        {
            return;
        }

        // The chain's levels add up: each counts for its share of the strength.
        const Uint32 lBloomLevels = (InSettings.BloomIntensity > 0.f && CanBloom()) ? BuildBloom(InCmd, InSettings) : 0u;
        const float  lBloom       = (lBloomLevels > 0) ? InSettings.BloomIntensity / static_cast<float>(lBloomLevels) : 0.f;

        const Vector4F lPost{ std::exp2(InSettings.ExposureStops), static_cast<float>(InSettings.Tonemapper), lBloom, 0.f };
        m_PostUBO->SetData(&lPost, static_cast<Uint32>(sizeof(lPost)));

        // Every pixel is written: nothing to clear.
        InCmd.BeginRenderPass(InOutput, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(*m_TonemapPipeline);
        m_Scene->BindColorTexture(0);

        // Without bloom the unit still holds a texture; the shader does not read it.
        if (lBloomLevels > 0)
        {
            m_BloomLevels[0]->BindColorTexture(1);
        }
        else
        {
            m_Scene->BindColorTexture(1);
        }

        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();
    }

    // =========================================================================
    // Bloom
    // =========================================================================
    Uint32 ScenePipeline2D::BuildBloom(ICommandBuffer& InCmd, const PostSettings& InSettings)
    {
        if (m_Scene == nullptr)
        {
            return 0;
        }

        const TDynArray<BloomLevel2D> lLevels = MakeBloomLevels2D(m_Scene->GetWidth(), m_Scene->GetHeight());
        const Uint32                  lCount  = static_cast<Uint32>(lLevels.size());

        // The chain follows the scene's size; levels past what it needs now are kept for later.
        for (Uint32 lIndex = 0; lIndex < lCount; ++lIndex)
        {
            if (lIndex >= m_BloomLevels.size())
            {
                FramebufferSpec lSpec;
                lSpec.Width        = lLevels[lIndex].Width;
                lSpec.Height       = lLevels[lIndex].Height;
                lSpec.DepthStencil = false;
                lSpec.ColorFormat  = ETextureFormat::RGBA16F;

                m_BloomLevels.push_back(m_Device->CreateFramebuffer(lSpec));
                m_BloomTargets.push_back(MakeUnique<OffscreenRenderTarget>(m_BloomLevels.back().get()));
            }
            else
            {
                m_BloomLevels[lIndex]->Resize(lLevels[lIndex].Width, lLevels[lIndex].Height);
            }
        }

        // Down: the bright light into the first level, then halved level by level.
        RunBloomPass(InCmd, BLOOM_PASS_FIRST_DOWN, InSettings, *m_Scene, *m_BloomTargets[0], false);
        for (Uint32 lIndex = 1; lIndex < lCount; ++lIndex)
        {
            RunBloomPass(InCmd, BLOOM_PASS_DOWN, InSettings, *m_BloomLevels[lIndex - 1], *m_BloomTargets[lIndex], false);
        }

        // Up: each level blurred into the one above it, added to what it holds.
        for (Uint32 lIndex = lCount - 1; lIndex > 0; --lIndex)
        {
            RunBloomPass(InCmd, BLOOM_PASS_UP, InSettings, *m_BloomLevels[lIndex], *m_BloomTargets[lIndex - 1], true);
        }

        return lCount;
    }

    void ScenePipeline2D::RunBloomPass(ICommandBuffer& InCmd, const Uint32 InMode, const PostSettings& InSettings,
                                       const IFramebuffer& InSource, IRenderTarget& InTarget, const bool bInAdd)
    {
        const Vector4F lPass{ static_cast<float>(InMode), InSettings.BloomThreshold, InSettings.BloomSoftness, 0.f };
        m_BloomUBO->SetData(&lPass, static_cast<Uint32>(sizeof(lPass)));

        // Written whole, or added onto: nothing to clear either way.
        InCmd.BeginRenderPass(InTarget, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(bInAdd ? *m_BloomAddPipeline : *m_BloomPipeline);
        InSource.BindColorTexture(0);
        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();
    }

    // =========================================================================
    // Shadows
    // =========================================================================
    IRenderTarget& ScenePipeline2D::PrepareOcclusion(const Uint32 InWidth, const Uint32 InHeight)
    {
        if (m_Occlusion == nullptr)
        {
            // Filtered, so the shadow pass finds the silhouettes between texels.
            FramebufferSpec lSpec;
            lSpec.Width        = InWidth;
            lSpec.Height       = InHeight;
            lSpec.DepthStencil = false;
            lSpec.ColorFormat  = ETextureFormat::R8;

            m_Occlusion       = m_Device->CreateFramebuffer(lSpec);
            m_OcclusionTarget = MakeUnique<OffscreenRenderTarget>(m_Occlusion.get());
        }
        else
        {
            m_Occlusion->Resize(InWidth, InHeight);
        }

        return *m_OcclusionTarget;
    }

    ITexture2D* ScenePipeline2D::BuildShadowMap(ICommandBuffer& InCmd, const ShadowBlock2D& InShadows)
    {
        if (!CanShadow() || m_Occlusion == nullptr || InShadows.GetCount() == 0)
        {
            return nullptr;
        }

        if (m_ShadowMap == nullptr)
        {
            // Distances, read back texel by texel: the sprite shader filters them itself.
            FramebufferSpec lSpec;
            lSpec.Width         = SHADOW_MAP_ANGLES_2D;
            lSpec.Height        = MAX_SHADOWED_LIGHTS_2D;
            lSpec.DepthStencil  = false;
            lSpec.ColorFormat   = ETextureFormat::R16F;
            lSpec.bLinearFilter = false;

            m_ShadowMap        = m_Device->CreateFramebuffer(lSpec);
            m_ShadowMapTarget  = MakeUnique<OffscreenRenderTarget>(m_ShadowMap.get());
            m_ShadowMapTexture = MakeUnique<FramebufferTexture>(*m_ShadowMap);
        }

        m_ShadowUBO->SetData(&InShadows, static_cast<Uint32>(sizeof(ShadowBlock2D)));

        // Every texel is written: nothing to clear.
        InCmd.BeginRenderPass(*m_ShadowMapTarget, ELoadOp::Load, Vector4F{ 1.f, 1.f, 1.f, 1.f });
        InCmd.BindPipeline(*m_ShadowPipeline);
        m_Occlusion->BindColorTexture(0);
        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();

        return m_ShadowMapTexture.get();
    }

    // =========================================================================
    // Ambient occlusion
    // =========================================================================
    ITexture2D* ScenePipeline2D::BuildAmbientOcclusion(ICommandBuffer& InCmd, const float InSigma)
    {
        if (!CanAmbientOcclusion() || m_Occlusion == nullptr)
        {
            return nullptr;
        }

        const Uint32 lWidth  = AmbientOcclusionExtent2D(m_Occlusion->GetWidth());
        const Uint32 lHeight = AmbientOcclusionExtent2D(m_Occlusion->GetHeight());

        for (Uint32 lIndex = 0; lIndex < 2; ++lIndex)
        {
            if (m_AmbientOcclusion[lIndex] == nullptr)
            {
                FramebufferSpec lSpec;
                lSpec.Width        = lWidth;
                lSpec.Height       = lHeight;
                lSpec.DepthStencil = false;
                lSpec.ColorFormat  = ETextureFormat::R8;

                m_AmbientOcclusion[lIndex]       = m_Device->CreateFramebuffer(lSpec);
                m_AmbientOcclusionTarget[lIndex] = MakeUnique<OffscreenRenderTarget>(m_AmbientOcclusion[lIndex].get());
            }
            else
            {
                m_AmbientOcclusion[lIndex]->Resize(lWidth, lHeight);
            }
        }

        if (m_AmbientOcclusionTexture == nullptr)
        {
            m_AmbientOcclusionTexture = MakeUnique<FramebufferTexture>(*m_AmbientOcclusion[0]);
        }

        // Shrink into [0], blur across into [1], blur down back into [0].
        RunAmbientOcclusionPass(InCmd, AO_PASS_SHRINK,      InSigma, *m_Occlusion,           *m_AmbientOcclusionTarget[0]);
        RunAmbientOcclusionPass(InCmd, AO_PASS_BLUR_ACROSS, InSigma, *m_AmbientOcclusion[0], *m_AmbientOcclusionTarget[1]);
        RunAmbientOcclusionPass(InCmd, AO_PASS_BLUR_DOWN,   InSigma, *m_AmbientOcclusion[1], *m_AmbientOcclusionTarget[0]);

        return m_AmbientOcclusionTexture.get();
    }

    void ScenePipeline2D::RunAmbientOcclusionPass(ICommandBuffer& InCmd, const Uint32 InMode, const float InSigma,
                                                  const IFramebuffer& InSource, IRenderTarget& InTarget)
    {
        const Vector4F lPass{ static_cast<float>(InMode), InSigma, 0.f, 0.f };
        m_AmbientOcclusionUBO->SetData(&lPass, static_cast<Uint32>(sizeof(lPass)));

        // Every texel is written: nothing to clear.
        InCmd.BeginRenderPass(InTarget, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(*m_AmbientOcclusionPipeline);
        InSource.BindColorTexture(0);
        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();
    }
}
