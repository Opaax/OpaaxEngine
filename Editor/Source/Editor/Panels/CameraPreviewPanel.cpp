#include "Editor/Panels/CameraPreviewPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Panels/EditorPanels.h"     // IsVisible — a hidden preview costs no pass
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"

#include "Renderer/CameraView.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec (created by the device)

#include "World/Components/CameraComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <imgui.h>

namespace Opaax::Editor
{
    CameraPreviewPanel::CameraPreviewPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    CameraPreviewPanel::~CameraPreviewPanel() = default;

    void CameraPreviewPanel::Startup()
    {
        // The engine's device builds the FBO (F2a) — the panel owns it, but never picks the backend.
        m_Framebuffer = m_Context.Engine.CreateFramebuffer(
            FramebufferSpec{ m_Size.x, m_Size.y, /*DepthStencil*/ true });

        if (m_Framebuffer == nullptr)
        {
            // Loud, not silent: the only other symptom is a permanently blank panel with a clean log.
            OPAAX_LOG(LogCameraPreviewPanel, Error, "CameraPreviewPanel startup — the engine created no "
                                                    "framebuffer; the preview will stay blank.");
            return;
        }

        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());

        OPAAX_LOG(LogCameraPreviewPanel, Info, "CameraPreviewPanel startup — offscreen FBO {}x{}", m_Size.x, m_Size.y);
    }

    void CameraPreviewPanel::OnPreRender()
    {
        ApplyPendingResize();
        SubmitView();
    }

    // =========================================================================
    // TryResolveCameraView — the primary selection, if it is a camera in the world being drawn.
    //
    // The same pair CameraManager::Resolve reads (a position from the transform, a size from the
    // component), so the preview and the game cannot disagree about what a camera means.
    // =========================================================================
    bool CameraPreviewPanel::TryResolveCameraView(CameraView& OutView) const
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        if (lWorld == nullptr)
        {
            return false;
        }

        Entity lEntity = m_Context.Selection.Get();

        // The camera must belong to the world being DRAWN: a pass renders the ACTIVE world, so an
        // Edit-world selection during PIE would frame the clone from a stranger's position.
        if (!lEntity.IsValid() || lEntity.GetWorld() != lWorld)
        {
            return false;
        }

        const TransformComponent* lTransform = lEntity.TryGet<TransformComponent>();
        const CameraComponent*    lCamera    = lEntity.TryGet<CameraComponent>();

        if (lTransform == nullptr || lCamera == nullptr)
        {
            return false;
        }

        OutView = CameraView{ lTransform->Position, lCamera->OrthoSize };

        return true;
    }

    void CameraPreviewPanel::SubmitView()
    {
        // A hidden preview draws nothing, so it must not cost a whole pass. OnPreRender runs either
        // way — this is the one panel where that matters, since the Viewport's target is what the
        // editor is looking at and this one's is not.
        if (m_RenderTarget == nullptr || !m_Context.Panels.IsVisible(PanelID()))
        {
            return;
        }

        CameraView lView;

        if (!TryResolveCameraView(lView))
        {
            return;
        }

        // No overlays: a preview of the game decorated like the editor is not a preview.
        m_Context.Engine.SubmitRenderView(*m_RenderTarget, lView, /*bInDrawOverlays*/ false);

        if (!m_bPreviewLogged)
        {
            m_bPreviewLogged = true;

            OPAAX_LOG(LogCameraPreviewPanel, Info, "Previewing a camera at ({}, {}), orthoSize {} — into a {}x{} target",
                      lView.Position.x, lView.Position.y, lView.OrthoSize, m_Size.x, m_Size.y);
        }
    }

    void CameraPreviewPanel::ApplyPendingResize()
    {
        if (m_PendingSize.x == 0 || m_PendingSize.y == 0)
        {
            return;
        }

        if (m_PendingSize.x == m_Size.x && m_PendingSize.y == m_Size.y)
        {
            return;
        }

        m_Size = m_PendingSize;

        if (m_Framebuffer != nullptr)
        {
            m_Framebuffer->Resize(m_Size.x, m_Size.y);
        }

        OPAAX_LOG(LogCameraPreviewPanel, Trace, "CameraPreviewPanel resized to {}x{}", m_Size.x, m_Size.y);
    }

    EditorImage CameraPreviewPanel::GetPreviewImage() const
    {
        return m_Framebuffer != nullptr ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer) : EditorImage{};
    }

    void CameraPreviewPanel::DrawContents()
    {
        // Measured here, applied by OnPreRender next frame — the deferred resize ViewportPanel
        // documents: reallocating the FBO between the world render and the sample would tear.
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();

        m_PendingSize.x = static_cast<Uint32>(lAvail.x);
        m_PendingSize.y = static_cast<Uint32>(lAvail.y);

        CameraView lView;

        if (!TryResolveCameraView(lView))
        {
            // Unity's answer, and it is the honest one: say WHY there is no picture. Drawing the
            // last frame's stale image instead would look like a camera that stopped moving.
            ImGui::TextDisabled("No camera");
            ImGui::TextDisabled("Select an entity with a CameraComponent.");

            return;
        }

        ImguiWidgets::Image(GetPreviewImage(), lAvail);
    }

    void CameraPreviewPanel::Shutdown()
    {
        // Nothing to unregister — the view is submitted per frame, so a panel that has stopped
        // running has already stopped being drawn.
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogCameraPreviewPanel, Info, "CameraPreviewPanel shutdown");
    }
}
