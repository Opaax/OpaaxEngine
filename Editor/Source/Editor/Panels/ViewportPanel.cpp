#include "Editor/Panels/ViewportPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer::Create + FramebufferSpec

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ViewportPanel::ViewportPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    ViewportPanel::~ViewportPanel() = default;

    void ViewportPanel::Startup()
    {
        m_Framebuffer  = IFramebuffer::Create(FramebufferSpec{ m_viewportSize.x, m_viewportSize.y, /*DepthStencil*/ true });
        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());

        m_Context.Engine.SetPrimaryRenderTarget(m_RenderTarget.get());

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel startup — offscreen FBO {}x{}", m_viewportSize.x, m_viewportSize.y)
    }

    void ViewportPanel::OnPreRender()
    {
        
        if (m_viewportPendingSize.x == 0 || m_viewportPendingSize.y == 0)
        {
            return;
        }
        if (m_viewportPendingSize.x == m_viewportSize.x && m_viewportPendingSize.y == m_viewportSize.y)
        {
            return;
        }

        m_viewportSize  = m_viewportPendingSize;
        
        if (m_Framebuffer != nullptr)
        {
            m_Framebuffer->Resize(m_viewportSize.x, m_viewportSize.y);
        }

        OPAAX_LOG(LogViewportPanel, Trace, "ViewportPanel resized to {}x{}", m_viewportSize.x, m_viewportSize.y)
    }

    EditorViewportImage ViewportPanel::GetViewportImage() const
    {
        return m_Framebuffer != nullptr ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer) : EditorViewportImage{};
    }

    void ViewportPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(m_viewportSizeDefault.x, m_viewportSizeDefault.y), ImGuiCond_FirstUseEver);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::Begin(m_Title.CStr());
        ImGui::PopStyleVar();

        // Measure the content region and cache it — OnPreRender applies it next frame (deferred resize).
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        m_viewportPendingSize.x     = static_cast<Uint32>(lAvail.x);
        m_viewportPendingSize.y     = static_cast<Uint32>(lAvail.y);
        
        const EditorViewportImage lImg = GetViewportImage();

        if (lImg.Handle != 0)
        {
            ImGui::Image(static_cast<ImTextureID>(lImg.Handle), lAvail,
                         ImVec2(lImg.UV0.x, lImg.UV0.y), ImVec2(lImg.UV1.x, lImg.UV1.y));
            
            if (!m_bImageLogged)
            {
                OPAAX_LOG(LogViewportPanel, Info, "Viewport displaying world FBO (handle={}, {}x{})", lImg.Handle, m_viewportSize.x, m_viewportSize.y)
                m_bImageLogged = true;
            }
        }
        else
        {
            ImGui::Dummy(lAvail);
        }

        ImGui::End();
    }

    void ViewportPanel::Shutdown()
    {
        // Clear the engine's primary target FIRST (while the engine is alive — LC/TearDown), so no live
        // frame reads a dangling target, THEN free the FBO (GL context still current). Order matters.
        m_Context.Engine.SetPrimaryRenderTarget(nullptr);
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel shutdown")
    }
}
