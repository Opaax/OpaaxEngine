#include "Renderer/RenderSystem.h"

#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RendererInfo.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"
#include "Renderer/Post/ScenePipeline2D.h"

#include "RHI/RHIDevice.h"
#include "RHI/IRHIDevice.h"
#include "RHI/ICommandBuffer.h"

namespace Opaax
{
    // =========================================================================
    // CTORS - DTORS (out-of-line: TUniquePtr members are forward-declared)
    // =========================================================================
    RenderSystem::RenderSystem()  = default;
    RenderSystem::~RenderSystem() { Shutdown(); }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool RenderSystem::Init(const RenderSystemDesc& InDesc)
    {
        if (InDesc.Surface == nullptr)
        {
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::Init — no surface in the desc.");
            return false;
        }

        m_Device = RHIDevice::Create(InDesc.Backend, *InDesc.Surface);
        if (!IsValidDevice())
        {
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::Init — backend produced no device.");
            return false;
        }

        m_ClearColor = InDesc.ClearColor;
        m_Backbuffer = MakeUnique<DefaultRenderTarget>(InDesc.Width, InDesc.Height);
        m_Device->SetViewport(0, 0, InDesc.Width, InDesc.Height);

        m_Renderer2D = MakeUnique<Renderer2D>();
        m_Renderer2D->Init(*m_Device, InDesc.Limits, InDesc.SpriteShader);

        m_ScenePipeline = MakeUnique<ScenePipeline2D>();
        m_ScenePipeline->Init(*m_Device, ScenePipelineShaders{ InDesc.TonemapShader, InDesc.ShadowShader,
                                                               InDesc.AmbientOcclusionShader, InDesc.BloomShader,
                                                               InDesc.FXAAShader });

        return true;
    }

    void RenderSystem::Shutdown()
    {
        if (IsValidDevice())
        {
            // Wait for the GPU before releasing resources.
            m_Device->WaitIdle();
        } 
        
        m_ScenePipeline.reset();
        m_Renderer2D.reset();
        m_Backbuffer.reset();
        m_Device.reset();
    }
    
    TUniquePtr<IFramebuffer> RenderSystem::CreateFramebuffer(const FramebufferSpec& InSpec)
    {
        if (!IsValidDevice())
        {
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::CreateFramebuffer — no device; no framebuffer created.");
            return nullptr;
        }

        return m_Device->CreateFramebuffer(InSpec);
    }

    TUniquePtr<ITexture2D> RenderSystem::CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        if (!IsValidDevice())
        {
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::CreateTexture — no device; no texture created.");
            return nullptr;
        }

        return m_Device->CreateTexture(InPixels, InWidth, InHeight, InChannels);
    }

    Uint32 RenderSystem::GetMaxSamples() const
    {
        return IsValidDevice() ? m_Device->GetCapabilities().MaxSamples : 1u;
    }

    SceneTargetPlan RenderSystem::PlanWorldView(const Uint32 InWidth, const Uint32 InHeight, const bool bInLit)
    {
        SceneTargetSettings lSettings = m_SceneTargetSettings;
        lSettings.bFXAA = lSettings.bFXAA && m_ScenePipeline != nullptr && m_ScenePipeline->CanFXAA();

        // What is used, not only what is asked: renderer.info answers this.
        const SceneTargetPlan lPlan = PlanSceneTarget(InWidth, InHeight, bInLit, lSettings, GetMaxSamples());
        m_WorldViews.push_back(lPlan);
        return lPlan;
    }

    void RenderSystem::SetTextureFilter(const ETextureFilter InFilter)
    {
        if (IsValidDevice())
        {
            m_Device->SetTextureFilter(InFilter);
            m_Renderer2D->SetTextureFilter(InFilter);
        }
    }

    void RenderSystem::BeginFrame()
    {
        if (!IsValidDevice())
        {
            return;
        }

        // Counters are per frame (a frame can have several passes).
        if (IsValidRenderer2D())
        {
            m_Renderer2D->ResetStats();
        }

        m_WorldViews.clear();

        m_Device->BeginFrame();
    }

    void RenderSystem::EndFrame()
    {
        if (!IsValidDevice())
        {
            return;
        }

        if (m_ScenePipeline != nullptr)
        {
            m_ScenePipeline->EndFrame();
        }

        m_Device->EndFrame();

        m_LastFrameWorldViews = m_WorldViews;

        // From one frame's end to the next: also counts what was resized between frames (an editor panel).
        const Uint64 lAllocations = m_Device->GetFramebufferAllocations();
        m_TargetsRecreated      = static_cast<Uint32>(lAllocations - m_AllocationsAtFrameEnd);
        m_AllocationsAtFrameEnd = lAllocations;
    }

    void RenderSystem::Present()
    {
        if (!IsValidDevice())
        {
            return;
        }

        m_Device->Present();
    }

    double RenderSystem::GetGpuFrameTimeMs() const
    {
        return IsValidDevice() ? m_Device->GetLastGpuFrameTimeMs() : -1.0;
    }

    RendererInfo RenderSystem::GetInfo() const
    {
        RendererInfo lInfo;

        if (!IsValidDevice())
        {
            return lInfo;
        }

        lInfo.bValid     = true;
        lInfo.Device     = m_Device->GetCapabilities();
        lInfo.GpuFrameMs = m_Device->GetLastGpuFrameTimeMs();

        lInfo.TextureFilter = m_Device->GetTextureFilter();
        lInfo.SceneSettings = m_SceneTargetSettings;
        lInfo.WorldViews    = m_LastFrameWorldViews;

        lInfo.TargetsRecreated      = m_TargetsRecreated;
        lInfo.TargetsAllocatedTotal = m_Device->GetFramebufferAllocations();
        lInfo.SceneTargets          = m_ScenePipeline != nullptr ? m_ScenePipeline->GetTargetCount() : 0;

        if (m_Backbuffer != nullptr)
        {
            lInfo.BackbufferWidth  = m_Backbuffer->GetWidth();
            lInfo.BackbufferHeight = m_Backbuffer->GetHeight();
        }

        return lInfo;
    }

    bool RenderSystem::CaptureBackbuffer(TDynArray<Uint8>& OutRGBA, Uint32& OutWidth, Uint32& OutHeight) const
    {
        if (!IsValidDevice() || m_Backbuffer == nullptr)
        {
            return false;
        }

        OutWidth  = m_Backbuffer->GetWidth();
        OutHeight = m_Backbuffer->GetHeight();
        return m_Device->ReadBackbufferPixels(OutWidth, OutHeight, OutRGBA);
    }

    ICommandBuffer* RenderSystem::GetCommandBuffer() const
    {
        return IsValidDevice() ? &m_Device->GetCommandBuffer() : nullptr;
    }

    void RenderSystem::BeginPass(IRenderTarget& InTarget, const RenderView& InView, const ELoadOp InLoadOp,
                                 const Vector4F* InClearColor)
    {
        if (!IsValidDevice() || !IsValidRenderer2D())
        {
            return;
        }

        m_Device->GetCommandBuffer().BeginRenderPass(InTarget, InLoadOp, InClearColor != nullptr ? *InClearColor : m_ClearColor);
        m_Renderer2D->BeginPass(InView, m_Device->GetCommandBuffer());
    }

    void RenderSystem::EndPass()
    {
        if (!IsValidDevice() || !IsValidRenderer2D())
        {
            return;
        }
        
        m_Renderer2D->EndPass();
        m_Device->GetCommandBuffer().EndRenderPass();
    }
    
    void RenderSystem::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (!m_Device || !m_Backbuffer)
        {
            return;
        }
        
        static_cast<DefaultRenderTarget*>(m_Backbuffer.get())->OnResize(InWidth, InHeight);
        
        m_Device->Resize(InWidth, InHeight);
    }
}
