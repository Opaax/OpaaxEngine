#include "ViewportPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#define GLAD_APIENTRY
#include <glad/glad.h>

#include "Core/Log/OpaaxLog.h"
#include "RHI/RenderCommand.h"

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

    void ViewportPanel::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        // NOTE: Viewport must match the FBO dimensions, not the GLFW window.
        //   This was the root cause of deformed sprites — the GL viewport was
        //   set to window size while the FBO was a different size.
        glViewport(0, 0,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
    }

    void ViewportPanel::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ViewportPanel::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        if (m_FBO) { Shutdown(); }

        m_Width  = InWidth;
        m_Height = InHeight;

        glGenTextures(1, &m_ColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenRenderbuffers(1, &m_DepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height));
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_ColorTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, m_DepthRBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            OPAAX_CORE_ERROR("ViewportPanel: FBO incomplete after resize to {}x{}!",
                InWidth, InHeight);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    static ImVec4 GetEditorStateBorderColor(EEditorState State)
    {
        switch (State)
        {
            case EEditorState::Playing: return ImVec4(0.30f, 0.70f, 0.30f, 1.f); // green
            case EEditorState::Paused:  return ImVec4(0.90f, 0.60f, 0.10f, 1.f); // amber
            case EEditorState::Editing:
            default:                    return ImVec4(0.40f, 0.40f, 0.40f, 1.f); // grey
        }
    }

    bool ViewportPanel::Draw(EEditorState State)
    {
        const ImVec4 lBorderColor = GetEditorStateBorderColor(State);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.f, 0.f));
        //ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.f);
        ImGui::PushStyleColor(ImGuiCol_Border, lBorderColor);

        ImGui::Begin("Viewport");

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        m_bHovered = ImGui::IsWindowHovered();
        m_bFocused = ImGui::IsWindowFocused();

        const ImVec2 lPanelSize = ImGui::GetContentRegionAvail();
        const Uint32 lNewW = static_cast<Uint32>(lPanelSize.x);
        const Uint32 lNewH = static_cast<Uint32>(lPanelSize.y);

        bool bResized = false;

        if (lNewW > 0 && lNewH > 0 && (lNewW != m_Width || lNewH != m_Height))
        {
            Resize(lNewW, lNewH);
            bResized = true;
            OPAAX_CORE_TRACE("ViewportPanel: resized to {}x{}", m_Width, m_Height);

            // [NEW] Fire callback so Camera2D and RenderCommand stay in sync.
            // Previously resize was silent — nobody updated the camera projection.
            if (OnResized)
            {
                OnResized(m_Width, m_Height);
            }
        }

        ImGui::Image(
            m_ColorTexture,
            lPanelSize,
            ImVec2(0.f, 1.f),
            ImVec2(1.f, 0.f)
        );

        ImGui::End();
        return bResized;
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR