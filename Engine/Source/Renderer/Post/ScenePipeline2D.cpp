#include "Renderer/Post/ScenePipeline2D.h"

#include "Core/Maths/Maths.h"

#include "Renderer/Lighting/Lighting2D.h"
#include "Renderer/RenderTarget.hpp"
#include "RHI/BindGroup.h"
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

        /** What the tonemap pass samples: the scene (unit 0) and the bloom (unit 1). */
        constexpr Uint32 TONEMAP_TEXTURES = 2;

        /** What a kept target holds (RenderTargetKey::Purpose): two alive at once in a view differ here. */
        constexpr Uint32 PURPOSE_SCENE               = 0;
        constexpr Uint32 PURPOSE_OCCLUSION           = 1;
        constexpr Uint32 PURPOSE_AMBIENT_OCCLUSION_A = 2;   // the result
        constexpr Uint32 PURPOSE_AMBIENT_OCCLUSION_B = 3;
        constexpr Uint32 PURPOSE_BLOOM_FIRST         = 4;   // then one per level

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

        /** What one post pass reads: its block and InTextures sampler slots, set before each draw. */
        TUniquePtr<IBindGroup> MakePassGroup(IRHIDevice& InDevice, IUniformBuffer& InBlock, const Uint32 InBinding,
                                             const Uint32 InTextures)
        {
            TUniquePtr<IBindGroup> lGroup = InDevice.CreateBindGroup(BindGroupLayout{ { InBinding }, InTextures });
            lGroup->SetUniformBuffer(InBlock);
            return lGroup;
        }
    }

    TDynArray<BloomLevel2D> MakeBloomLevels2D(const Uint32 InWidth, const Uint32 InHeight)
    {
        TDynArray<BloomLevel2D> lLevels;

        BloomLevel2D lLevel{ Maths::Max(InWidth / 2u, 1u), Maths::Max(InHeight / 2u, 1u) };
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
        m_TonemapGroup    = MakePassGroup(InDevice, *m_PostUBO, POST_UBO_BINDING, TONEMAP_TEXTURES);

        if (HasStages(InShaders.Shadow))
        {
            m_ShadowShader   = InDevice.CreateShader(InShaders.Shadow);
            m_ShadowPipeline = MakeFullscreenPipeline(InDevice, *m_ShadowShader, "ScenePipeline2D::Shadow");
            m_ShadowUBO      = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(ShadowBlock2D)), SHADOW_UBO_BINDING);
            m_ShadowGroup    = MakePassGroup(InDevice, *m_ShadowUBO, SHADOW_UBO_BINDING, 1);
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
            m_AmbientOcclusionGroup    = MakePassGroup(InDevice, *m_AmbientOcclusionUBO, AMBIENT_OCCLUSION_UBO_BINDING, 1);
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
            m_BloomGroup       = MakePassGroup(InDevice, *m_BloomUBO, BLOOM_UBO_BINDING, 1);
        }
        else
        {
            OPAAX_LOG(LogScenePipeline2D, Warn, "No bloom shader: environments get none");
        }

        return true;
    }

    void ScenePipeline2D::Shutdown()
    {
        // The groups first: they point at what follows.
        m_TonemapGroup.reset();
        m_ShadowGroup.reset();
        m_AmbientOcclusionGroup.reset();
        m_BloomGroup.reset();

        m_Scene     = nullptr;
        m_Occlusion = nullptr;
        m_BloomLevels.clear();
        m_Targets.Clear();

        m_TonemapPipeline.reset();   // before its shader
        m_TonemapShader.reset();
        m_PostUBO.reset();

        m_ShadowMapTexture.reset();
        m_ShadowMapTarget.reset();
        m_ShadowMap.reset();
        m_ShadowPipeline.reset();
        m_ShadowShader.reset();
        m_ShadowUBO.reset();

        m_AmbientOcclusionPipeline.reset();
        m_AmbientOcclusionShader.reset();
        m_AmbientOcclusionUBO.reset();

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
    void ScenePipeline2D::EndFrame()
    {
        // A target may be released here: nothing points at one between frames.
        m_Scene     = nullptr;
        m_Occlusion = nullptr;
        m_BloomLevels.clear();

        m_Targets.EndFrame();
    }

    ScenePipeline2D::PostTarget& ScenePipeline2D::AcquireTarget(const Uint32 InPurpose, const Uint32 InWidth,
                                                                const Uint32 InHeight, const ETextureFormat InFormat,
                                                                const Uint32 InSamples)
    {
        RenderTargetKey lKey;
        lKey.Purpose = InPurpose;
        lKey.Width   = Maths::Max(InWidth, 1u);
        lKey.Height  = Maths::Max(InHeight, 1u);
        lKey.Format  = InFormat;
        lKey.Samples = Maths::Max(InSamples, 1u);

        return m_Targets.Acquire(lKey,
            [this](const RenderTargetKey& InKey)
            {
                FramebufferSpec lSpec;
                lSpec.Width         = InKey.Width;
                lSpec.Height        = InKey.Height;
                lSpec.ColorFormat   = InKey.Format;
                lSpec.bLinearFilter = InKey.bLinearFilter;
                lSpec.Samples       = InKey.Samples;

                TUniquePtr<PostTarget> lTarget = MakeUnique<PostTarget>();
                lTarget->Framebuffer = m_Device->CreateFramebuffer(lSpec);
                lTarget->Target      = MakeUnique<OffscreenRenderTarget>(lTarget->Framebuffer.get());
                lTarget->Texture     = MakeUnique<FramebufferTexture>(*lTarget->Framebuffer);
                return lTarget;
            },
            [](PostTarget& InTarget, const RenderTargetKey& InKey)
            {
                InTarget.Framebuffer->Resize(InKey.Width, InKey.Height);
            });
    }

    IRenderTarget& ScenePipeline2D::PrepareScene(const Uint32 InWidth, const Uint32 InHeight, const ETextureFormat InFormat,
                                                 const Uint32 InSamples)
    {
        m_Scene = &AcquireTarget(PURPOSE_SCENE, InWidth, InHeight, InFormat, InSamples);
        return *m_Scene->Target;
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

        const Vector4F lPost{ Maths::Exp2(InSettings.ExposureStops), static_cast<float>(InSettings.Tonemapper), lBloom, 0.f };

        // Without bloom the unit still holds a texture; the shader does not read it.
        RunTonemapPass(InCmd, InOutput, lPost, lBloomLevels > 0 ? *m_BloomLevels[0]->Framebuffer : *m_Scene->Framebuffer);
    }

    void ScenePipeline2D::CopyScene(ICommandBuffer& InCmd, IRenderTarget& InOutput)
    {
        if (!IsReady() || m_Scene == nullptr)
        {
            return;
        }

        // The tonemap shader's copy mode (w): no exposure, no curve, no gamma, alpha kept.
        RunTonemapPass(InCmd, InOutput, Vector4F{ 1.f, 0.f, 0.f, 1.f }, *m_Scene->Framebuffer);
    }

    void ScenePipeline2D::RunTonemapPass(ICommandBuffer& InCmd, IRenderTarget& InOutput, const Vector4F& InPost,
                                         const IFramebuffer& InBloom)
    {
        m_PostUBO->SetData(&InPost, static_cast<Uint32>(sizeof(InPost)));

        // Every pixel is written: nothing to clear.
        InCmd.BeginRenderPass(InOutput, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(*m_TonemapPipeline);

        m_TonemapGroup->SetTexture(0, *m_Scene->Framebuffer);
        m_TonemapGroup->SetTexture(1, InBloom);
        InCmd.BindBindGroup(*m_TonemapGroup);

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

        const TDynArray<BloomLevel2D> lLevels = MakeBloomLevels2D(m_Scene->Framebuffer->GetWidth(),
                                                                  m_Scene->Framebuffer->GetHeight());
        const Uint32                  lCount  = static_cast<Uint32>(lLevels.size());

        // The chain of this scene's size.
        m_BloomLevels.clear();
        for (Uint32 lIndex = 0; lIndex < lCount; ++lIndex)
        {
            m_BloomLevels.push_back(&AcquireTarget(PURPOSE_BLOOM_FIRST + lIndex, lLevels[lIndex].Width,
                                                   lLevels[lIndex].Height, ETextureFormat::RGBA16F));
        }

        // Down: the bright light into the first level, then halved level by level.
        RunBloomPass(InCmd, BLOOM_PASS_FIRST_DOWN, InSettings, *m_Scene->Framebuffer, *m_BloomLevels[0]->Target, false);
        for (Uint32 lIndex = 1; lIndex < lCount; ++lIndex)
        {
            RunBloomPass(InCmd, BLOOM_PASS_DOWN, InSettings, *m_BloomLevels[lIndex - 1]->Framebuffer,
                         *m_BloomLevels[lIndex]->Target, false);
        }

        // Up: each level blurred into the one above it, added to what it holds.
        for (Uint32 lIndex = lCount - 1; lIndex > 0; --lIndex)
        {
            RunBloomPass(InCmd, BLOOM_PASS_UP, InSettings, *m_BloomLevels[lIndex]->Framebuffer,
                         *m_BloomLevels[lIndex - 1]->Target, true);
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
        m_BloomGroup->SetTexture(0, InSource);
        InCmd.BindBindGroup(*m_BloomGroup);
        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();
    }

    // =========================================================================
    // Shadows
    // =========================================================================
    IRenderTarget& ScenePipeline2D::PrepareOcclusion(const Uint32 InWidth, const Uint32 InHeight)
    {
        // Filtered, so the shadow pass finds the silhouettes between texels.
        m_Occlusion = &AcquireTarget(PURPOSE_OCCLUSION, InWidth, InHeight, ETextureFormat::R8);
        return *m_Occlusion->Target;
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
        m_ShadowGroup->SetTexture(0, *m_Occlusion->Framebuffer);
        InCmd.BindBindGroup(*m_ShadowGroup);
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

        const Uint32 lWidth  = AmbientOcclusionExtent2D(m_Occlusion->Framebuffer->GetWidth());
        const Uint32 lHeight = AmbientOcclusionExtent2D(m_Occlusion->Framebuffer->GetHeight());

        PostTarget& lResult = AcquireTarget(PURPOSE_AMBIENT_OCCLUSION_A, lWidth, lHeight, ETextureFormat::R8);
        PostTarget& lOther  = AcquireTarget(PURPOSE_AMBIENT_OCCLUSION_B, lWidth, lHeight, ETextureFormat::R8);

        // Shrink into the result, blur across into the other, blur down back into the result.
        RunAmbientOcclusionPass(InCmd, AO_PASS_SHRINK,      InSigma, *m_Occlusion->Framebuffer, *lResult.Target);
        RunAmbientOcclusionPass(InCmd, AO_PASS_BLUR_ACROSS, InSigma, *lResult.Framebuffer,      *lOther.Target);
        RunAmbientOcclusionPass(InCmd, AO_PASS_BLUR_DOWN,   InSigma, *lOther.Framebuffer,       *lResult.Target);

        return lResult.Texture.get();
    }

    void ScenePipeline2D::RunAmbientOcclusionPass(ICommandBuffer& InCmd, const Uint32 InMode, const float InSigma,
                                                  const IFramebuffer& InSource, IRenderTarget& InTarget)
    {
        const Vector4F lPass{ static_cast<float>(InMode), InSigma, 0.f, 0.f };
        m_AmbientOcclusionUBO->SetData(&lPass, static_cast<Uint32>(sizeof(lPass)));

        // Every texel is written: nothing to clear.
        InCmd.BeginRenderPass(InTarget, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(*m_AmbientOcclusionPipeline);
        m_AmbientOcclusionGroup->SetTexture(0, InSource);
        InCmd.BindBindGroup(*m_AmbientOcclusionGroup);
        InCmd.DrawFullscreen();
        InCmd.EndRenderPass();
    }
}
