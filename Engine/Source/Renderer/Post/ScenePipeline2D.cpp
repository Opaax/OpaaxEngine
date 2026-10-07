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

        /** The modes of AmbientOcclusion2D.glsl. */
        constexpr Uint32 AO_PASS_SHRINK      = 0;
        constexpr Uint32 AO_PASS_BLUR_ACROSS = 1;
        constexpr Uint32 AO_PASS_BLUR_DOWN   = 2;

        bool HasStages(const ShaderDesc& InShader) noexcept
        {
            return !InShader.VertexSrc.IsEmpty() && !InShader.FragmentSrc.IsEmpty();
        }

        /** A fullscreen pass over a target, without blending: every pixel written. */
        TUniquePtr<IPipeline> MakeFullscreenPipeline(IRHIDevice& InDevice, IShader& InShader, const char* InName)
        {
            PipelineDesc lDesc;
            lDesc.Shader    = &InShader;
            lDesc.Blend     = EBlendMode::None;
            lDesc.DebugName = InName;
            return InDevice.CreatePipeline(lDesc);
        }
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

        const Vector4F lPost{ std::exp2(InSettings.ExposureStops), static_cast<float>(InSettings.Tonemapper), 0.f, 0.f };
        m_PostUBO->SetData(&lPost, static_cast<Uint32>(sizeof(lPost)));

        // Every pixel is written: nothing to clear.
        InCmd.BeginRenderPass(InOutput, ELoadOp::Load, Vector4F{ 0.f, 0.f, 0.f, 1.f });
        InCmd.BindPipeline(*m_TonemapPipeline);
        m_Scene->BindColorTexture(0);
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
