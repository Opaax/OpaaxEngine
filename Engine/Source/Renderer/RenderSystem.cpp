#include "RenderSystem.h"

#include "Application/Services/ILogger.h"

#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Renderer2D.h"

#include "RHI/RHIDevice.h"
#include "RHI/IRHIDevice.h"
#include "RHI/ICommandBuffer.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(RenderSystem)

    // =========================================================================
    // CTORS - DTORS (out-of-line — owned UniquePtr members are forward-declared)
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
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::Init — no surface in the desc.")
            return false;
        }

        m_Device = RHIDevice::Create(InDesc.Backend, *InDesc.Surface);
        if (!m_Device)
        {
            OPAAX_LOG(LogRenderSystem, Error, "RenderSystem::Init — backend produced no device.")
            return false;
        }

        m_ClearColor = InDesc.ClearColor;
        m_Backbuffer = MakeUnique<DefaultRenderTarget>(InDesc.Width, InDesc.Height);
        m_Device->SetViewport(0, 0, InDesc.Width, InDesc.Height);

        m_Renderer2D = MakeUnique<Renderer2D>();
        m_Renderer2D->Init(*m_Device, InDesc.Limits, InDesc.SpriteShader);

        OPAAX_LOG(LogRenderSystem, Info, "RenderSystem started.")
        return true;
    }

    void RenderSystem::Shutdown()
    {
        if (m_Device) { m_Device->WaitIdle(); } // GPU-idle before dropping GPU resources
        m_Renderer2D.reset();                   // release the batcher before the device/surface
        m_Backbuffer.reset();
        m_Device.reset();
    }

    void RenderSystem::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (!m_Device || !m_Backbuffer) { return; }
        static_cast<DefaultRenderTarget*>(m_Backbuffer.get())->OnResize(InWidth, InHeight);
        m_Device->Resize(InWidth, InHeight);
    }

    // =========================================================================
    // Frame
    // =========================================================================
    void RenderSystem::BeginFrame()
    {
        if (!m_Device || !m_Backbuffer) { return; }
        m_Device->BeginFrame();
        m_Device->GetCommandBuffer().BeginRenderPass(*m_Backbuffer, ELoadOp::Clear, m_ClearColor);
    }

    void RenderSystem::EndFrame()
    {
        if (!m_Device) { return; }
        m_Device->GetCommandBuffer().EndRenderPass();
        m_Device->EndFrame();
        // NOTE: present moved OUT of here (S7). The frame is submitted; the host shows it via
        // Present() after TickFrame, so the editor can draw UI to the backbuffer in between.
    }

    void RenderSystem::Present()
    {
        if (!m_Device) { return; }
        m_Device->Present();   // device owns HOW; the host decides WHEN (Editor.md D2)
    }

    void RenderSystem::BeginScene(const RenderView& InView)
    {
        if (!m_Device || !m_Renderer2D) { return; }
        m_Renderer2D->BeginScene(InView, m_Device->GetCommandBuffer());
    }

    void RenderSystem::EndScene()
    {
        if (m_Renderer2D) { m_Renderer2D->End(); }
    }
}
