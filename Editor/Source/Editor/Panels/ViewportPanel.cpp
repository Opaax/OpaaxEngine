#include "Editor/Panels/ViewportPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSelection.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include "Renderer/DebugDraw.h"             // selection outline (M2c)
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec (created by the device)

#include "World/Components/DummyComponent.h"
#include "World/Entity/Entity.h"

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
        // The engine's device builds the FBO (F2a) — the panel owns it, but never picks the backend.
        m_Framebuffer = m_Context.Engine.CreateFramebuffer(
            FramebufferSpec{ m_viewportSize.x, m_viewportSize.y, /*DepthStencil*/ true });

        if (m_Framebuffer == nullptr)
        {
            // Loud, not silent: without this the only symptom is Draw()'s Dummy fallback — a blank
            // panel and a clean log, which is exactly the failure L15 is about.
            OPAAX_LOG(LogViewportPanel, Error, "ViewportPanel startup — the engine created no framebuffer; "
                                               "the viewport will stay blank.")
            return;
        }

        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());

        m_Context.Engine.SetPrimaryRenderTarget(m_RenderTarget.get());

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel startup — offscreen FBO {}x{}", m_viewportSize.x, m_viewportSize.y)
    }

    // =========================================================================
    // OnPreRender — runs in EditorService::BeginFrame, i.e. BEFORE Engine().Loop() renders the world.
    // Everything here reaches this frame's render; anything done in Draw() would be one frame late.
    //
    // Both steps must run independently: ApplyPendingResize early-outs on the steady state (no
    // pending resize), which is every frame that isn't a resize — so the outline cannot ride at the
    // end of that body or it would almost never be queued.
    // =========================================================================
    void ViewportPanel::OnPreRender()
    {
        ApplyPendingResize();
        EnqueueSelectionOutline();
    }

    void ViewportPanel::ApplyPendingResize()
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

    // =========================================================================
    // EnqueueSelectionOutline — mark the selected entity in the world itself, not just as a
    // highlighted Hierarchy row. Re-submitted every frame by design: DebugDraw is drained and
    // cleared by the renderer each frame, so "still selected" means "queue it again".
    //
    // DummyComponent is what carries position/size and the only thing RendererManager draws — the
    // same component the Inspector edits (M2b). A real TransformComponent is M3's work.
    // =========================================================================
    void ViewportPanel::EnqueueSelectionOutline()
    {
        // A local COPY of the handle (the InspectorPanel idiom) — Entity is a value type, and TryGet
        // is non-const.
        Entity lSelected = m_Context.Selection.Get();
        if (!lSelected.IsValid())
        {
            return;
        }

        const DummyComponent* lComp = lSelected.TryGet<DummyComponent>();
        if (lComp == nullptr)
        {
            return;
        }

        m_Context.Engine.GetDebugDraw().DrawBox(lComp->Position, lComp->Size + m_OutlinePadding,
                                                m_OutlineColor, m_OutlineThickness);

        // Log the SUCCESS branch (L15): both returns above are silent, so "selected an entity with no
        // DummyComponent" would otherwise be indistinguishable from a broken DebugDraw pipe. One-shot
        // — this runs every frame.
        if (!m_bOutlineLogged)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Selection outline enqueued (4 debug lines around {},{})",
                      lComp->Position.x, lComp->Position.y)
            m_bOutlineLogged = true;
        }
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
