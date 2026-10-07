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
    bool ScenePipeline2D::Init(IRHIDevice& InDevice, const ShaderDesc& InTonemapShader, const ShaderDesc& InShadowShader)
    {
        if (InTonemapShader.VertexSrc.IsEmpty() || InTonemapShader.FragmentSrc.IsEmpty())
        {
            OPAAX_LOG(LogScenePipeline2D, Error, "No tonemap shader: worlds with an Environment are drawn without HDR");
            return false;
        }

        m_Device = &InDevice;

        m_TonemapShader   = InDevice.CreateShader(InTonemapShader);
        m_TonemapPipeline = MakeFullscreenPipeline(InDevice, *m_TonemapShader, "ScenePipeline2D::Tonemap");
        m_PostUBO         = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(Vector4F)), POST_UBO_BINDING);

        if (InShadowShader.VertexSrc.IsEmpty() || InShadowShader.FragmentSrc.IsEmpty())
        {
            OPAAX_LOG(LogScenePipeline2D, Warn, "No shadow shader: lights cast no shadows");
            return true;
        }

        m_ShadowShader   = InDevice.CreateShader(InShadowShader);
        m_ShadowPipeline = MakeFullscreenPipeline(InDevice, *m_ShadowShader, "ScenePipeline2D::Shadow");
        m_ShadowUBO      = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(ShadowBlock2D)), SHADOW_UBO_BINDING);
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
}
