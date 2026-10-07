#include "Renderer/Post/ScenePipeline2D.h"

#include <cmath>

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
    }

    ScenePipeline2D::ScenePipeline2D()  = default;
    ScenePipeline2D::~ScenePipeline2D() { Shutdown(); }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool ScenePipeline2D::Init(IRHIDevice& InDevice, const ShaderDesc& InTonemapShader)
    {
        if (InTonemapShader.VertexSrc.IsEmpty() || InTonemapShader.FragmentSrc.IsEmpty())
        {
            OPAAX_LOG(LogScenePipeline2D, Error, "No tonemap shader: worlds with an Environment are drawn without HDR");
            return false;
        }

        m_Device = &InDevice;

        m_TonemapShader = InDevice.CreateShader(InTonemapShader);

        PipelineDesc lDesc;
        lDesc.Shader    = m_TonemapShader.get();
        lDesc.Blend     = EBlendMode::None;
        lDesc.DebugName = "ScenePipeline2D::Tonemap";
        m_TonemapPipeline = InDevice.CreatePipeline(lDesc);

        m_PostUBO = InDevice.CreateUniformBuffer(static_cast<Uint32>(sizeof(Vector4F)), POST_UBO_BINDING);
        return true;
    }

    void ScenePipeline2D::Shutdown()
    {
        m_SceneTarget.reset();
        m_Scene.reset();
        m_TonemapPipeline.reset();   // before its shader
        m_TonemapShader.reset();
        m_PostUBO.reset();
        m_Device = nullptr;
    }

    bool ScenePipeline2D::IsReady() const noexcept
    {
        return m_Device != nullptr && m_TonemapPipeline != nullptr && m_PostUBO != nullptr;
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
}
