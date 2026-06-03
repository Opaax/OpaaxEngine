#include "ViewportPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Core/Log/OpaaxLog.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "RHI/RenderAPI.h"

namespace Opaax::Editor
{
    bool ViewportPanel::Startup()
    {
        // Build the offscreen target via the RHI factory — no raw GL in this panel.
        // Real size arrives on the first Draw's content-region resize.
        m_Framebuffer = IFramebuffer::Create(FramebufferSpec{ m_Width, m_Height, /*DepthStencil*/ true });
        OPAAX_CORE_INFO("ViewportPanel::Startup() — FBO {}x{}", m_Width, m_Height);
        return m_Framebuffer != nullptr;
    }

    void ViewportPanel::Shutdown()
    {
        m_Framebuffer.reset();
        OPAAX_CORE_INFO("ViewportPanel::Shutdown()");
    }

    void ViewportPanel::Bind()
    {
        if (m_Framebuffer) { m_Framebuffer->Bind(); }
    }

    void ViewportPanel::Unbind()
    {
        if (m_Framebuffer) { m_Framebuffer->Unbind(); }
    }

    void ViewportPanel::Resize(Uint32 InWidth, Uint32 InHeight)
    {
        m_Width  = InWidth;
        m_Height = InHeight;
        if (m_Framebuffer) { m_Framebuffer->Resize(InWidth, InHeight); }
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

    bool ViewportPanel::Draw(EEditorState State, IEditorUIBackend& InUIBackend)
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

        // Cache the content region's screen-absolute top-left. Under ConfigFlags_ViewportsEnable
        // both GetWindowPos() and GetMousePos() return screen-absolute coords, so this works
        // identically whether the Viewport panel is docked in the main window or undocked into
        // its own OS window. Subtracting this from a mouse pos yields viewport-local pixels.
        const ImVec2 lWinPos        = ImGui::GetWindowPos();
        const ImVec2 lContentOffset = ImGui::GetWindowContentRegionMin();
        m_ContentScreenPos          = { lWinPos.x + lContentOffset.x, lWinPos.y + lContentOffset.y };

        const ImVec2 lPanelSize = ImGui::GetContentRegionAvail();
        const Uint32 lNewW = static_cast<Uint32>(lPanelSize.x);
        const Uint32 lNewH = static_cast<Uint32>(lPanelSize.y);

        bool bResized = false;

        if (lNewW > 0 && lNewH > 0 && (lNewW != m_Width || lNewH != m_Height))
        {
            Resize(lNewW, lNewH);
            bResized = true;
            OPAAX_CORE_TRACE("ViewportPanel: resized to {}x{}", m_Width, m_Height);

            // Forward resize so the active camera and RenderCommand viewport stay in sync.
            if (OnResized)
            {
                OnResized(m_Width, m_Height);
            }
        }

        // Resolve the backend-specific ImGui texture handle for the offscreen color image (GL tex
        // name, or a Vulkan descriptor set).
        const ImTextureID lViewportTex = m_Framebuffer
            ? static_cast<ImTextureID>(InUIBackend.GetViewportTextureID(*m_Framebuffer))
            : static_cast<ImTextureID>(0);

        // Sample the image upright for each backend's storage convention: the GL FBO is bottom-up
        // (V flipped), while the Vulkan offscreen image is top-down with the negative-viewport
        // render (sampled straight). Without the per-backend UVs the viewport renders inverted.
        const bool   lVulkan = (RenderAPI::GetBackend() == EBackend::Vulkan);
        const ImVec2 lUV0    = lVulkan ? ImVec2(0.f, 0.f) : ImVec2(0.f, 1.f);
        const ImVec2 lUV1    = lVulkan ? ImVec2(1.f, 1.f) : ImVec2(1.f, 0.f);

        ImGui::Image(lViewportTex, lPanelSize, lUV0, lUV1);

        ImGui::End();
        return bResized;
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR