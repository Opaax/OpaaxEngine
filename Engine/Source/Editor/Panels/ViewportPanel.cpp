#include "ViewportPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#define GLAD_APIENTRY
#include <glad/glad.h>

#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    bool ViewportPanel::Startup()
    {
        Resize(m_Width, m_Height);
        OPAAX_CORE_INFO("ViewportPanel::Startup() — FBO {}x{}", m_Width, m_Height);
        return m_FBO != 0;
    }

    void ViewportPanel::Shutdown()
    {
        if (m_ColorTexture) { glDeleteTextures(1, &m_ColorTexture);      m_ColorTexture = 0; }
        if (m_DepthRBO)     { glDeleteRenderbuffers(1, &m_DepthRBO);     m_DepthRBO     = 0; }
        if (m_FBO)          { glDeleteFramebuffers(1, &m_FBO);           m_FBO          = 0; }

        OPAAX_CORE_INFO("ViewportPanel::Shutdown()");
    }

    void ViewportPanel::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (m_FBO)
        {
            Shutdown();
        }

        m_Width  = InWidth;
        m_Height = InHeight;

        // Color texture
        glGenTextures(1, &m_ColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Depth renderbuffer
        glGenRenderbuffers(1, &m_DepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        // FBO
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_ColorTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, m_DepthRBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            OPAAX_CORE_ERROR("ViewportPanel: FBO incomplete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ViewportPanel::BindFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
    }

    void ViewportPanel::UnbindFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ViewportPanel::Draw()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::Begin("Viewport");
        ImGui::PopStyleVar();

        m_bHovered = ImGui::IsWindowHovered();
        m_bFocused = ImGui::IsWindowFocused();

        // Check if panel was resized
        const ImVec2 lPanelSize = ImGui::GetContentRegionAvail();
        const Uint32 lNewW = static_cast<Uint32>(lPanelSize.x);
        const Uint32 lNewH = static_cast<Uint32>(lPanelSize.y);

        if (lNewW > 0 && lNewH > 0 && (lNewW != m_Width || lNewH != m_Height))
        {
            Resize(lNewW, lNewH);
            OPAAX_CORE_TRACE("ViewportPanel: resized to {}x{}", m_Width, m_Height);
        }

        // Display color texture — ImGui uses top-left origin, OpenGL bottom-left.
        // Flip UV Y to correct orientation.
        ImGui::Image(
            m_ColorTexture,
            lPanelSize,
            ImVec2(0.f, 1.f),   // UV top-left
            ImVec2(1.f, 0.f)    // UV bottom-right (flipped Y)
        );

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR