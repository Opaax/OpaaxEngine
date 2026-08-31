#include "RenderSystem.h"

#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"

#include "RHI/RHIDevice.h"
#include "RHI/IRHIDevice.h"
#include "RHI/ICommandBuffer.h"

namespace Opaax
{
    // =========================================================================
    // CTORS - DTORS (out-of-line — owned TUniquePtr members are forward-declared)
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

        OPAAX_LOG(LogRenderSystem, Info, "RenderSystem started.");
        return true;
    }

    void RenderSystem::Shutdown()
    {
        if (IsValidDevice())
        {
            // GPU-idle before dropping GPU resources
            m_Device->WaitIdle();
        } 
        
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

    void RenderSystem::BeginFrame()
    {
        if (!IsValidDevice())
        {
            return;
        }
        
        m_Device->BeginFrame();
    }

    void RenderSystem::EndFrame()
    {
        if (!IsValidDevice())
        {
            return;
        }
        
        m_Device->EndFrame();
    }

    void RenderSystem::Present()
    {
        if (!IsValidDevice())
        {
            return;
        }

        m_Device->Present();
    }

    void RenderSystem::BeginPass(IRenderTarget& InTarget, const RenderView& InView)
    {
        if (!IsValidDevice() || !IsValidRenderer2D())
        {
            return;
        }
        
        m_Device->GetCommandBuffer().BeginRenderPass(InTarget, ELoadOp::Clear, m_ClearColor);
        m_Renderer2D->BeginScene(InView, m_Device->GetCommandBuffer());
    }

    void RenderSystem::EndPass()
    {
        if (!IsValidDevice() || !IsValidRenderer2D())
        {
            return;
        }
        
        m_Renderer2D->End();
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
