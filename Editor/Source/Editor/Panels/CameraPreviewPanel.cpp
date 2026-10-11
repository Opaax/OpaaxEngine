#include "Editor/Panels/CameraPreviewPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Panels/EditorPanels.h"     // IsVisible
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"

#include "Renderer/CameraView.h"
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec

#include "Renderer/Camera/CameraComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
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
        // The engine's device creates the FBO; the panel owns it.
        m_Framebuffer = m_Context.Engine.CreateFramebuffer(
            FramebufferSpec{ m_Size.x, m_Size.y, /*DepthStencil*/ true });

        if (m_Framebuffer == nullptr)
        {
            // Log it: otherwise the panel would just stay blank.
            OPAAX_LOG(LogCameraPreviewPanel, Error, "CameraPreviewPanel startup — the engine created no "
                                                    "framebuffer; the preview will stay blank.");
            return;
        }

        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
    }

    void CameraPreviewPanel::OnPreRender()
    {
        // Before the submit, so a camera selected this frame is shown this frame.
        TrackSelection();

        ApplyPendingResize();
        SubmitView();
    }

    // =========================================================================
    // TrackSelection — a selected camera becomes the previewed one; other selections change nothing.
    //   Uses the primary selection (what the Inspector shows).
    // =========================================================================
    void CameraPreviewPanel::TrackSelection()
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        if (lWorld == nullptr)
        {
            return;
        }

        Entity lSelected = m_Context.Selection.Get();

        // The camera must be in the active world (the one being drawn).
        if (!lSelected.IsValid() || lSelected.GetWorld() != lWorld)
        {
            return;
        }

        if (lSelected.TryGet<TransformComponent>() == nullptr || lSelected.TryGet<CameraComponent>() == nullptr)
        {
            return;
        }

        m_Previewed = lSelected.GetHandle();
    }

    // =========================================================================
    // TryResolveCameraView — the previewed entity, if it is still a camera in the active world.
    //   Reads the same values as CameraManager::Resolve.
    // =========================================================================
    bool CameraPreviewPanel::TryResolveCameraView(CameraView& OutView) const
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        if (lWorld == nullptr || m_Previewed == ENTITY_NONE)
        {
            return false;
        }

        // Resolved in the active world (TrackSelection only stores entities of the active world).
        Entity lEntity{ m_Previewed, lWorld };

        if (!lEntity.IsValid())
        {
            return false;
        }

        const TransformComponent* lTransform = lEntity.TryGet<TransformComponent>();
        const CameraComponent*    lCamera    = lEntity.TryGet<CameraComponent>();

        if (lTransform == nullptr || lCamera == nullptr)
        {
            return false;
        }

        // World pose, like CameraManager::Resolve.
        OutView = CameraView{ EntityHierarchy::WorldTransform(lEntity).Position, lCamera->OrthoSize };

        return true;
    }

    void CameraPreviewPanel::OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/)
    {
        m_Previewed = ENTITY_NONE;
    }

    void CameraPreviewPanel::SubmitView()
    {
        // A hidden preview draws nothing and costs no pass.
        if (m_RenderTarget == nullptr || !m_Context.Panels.IsVisible(PanelID()))
        {
            return;
        }

        CameraView lView;

        if (!TryResolveCameraView(lView))
        {
            return;
        }

        // No overlays: the preview shows what the game shows.
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
    }

    EditorImage CameraPreviewPanel::GetPreviewImage() const
    {
        return m_Framebuffer != nullptr ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer) : EditorImage{};
    }

    void CameraPreviewPanel::DrawContents()
    {
        // Measured here, applied by OnPreRender next frame (resizing between render and sample would tear).
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();

        m_PendingSize.x = static_cast<Uint32>(lAvail.x);
        m_PendingSize.y = static_cast<Uint32>(lAvail.y);

        CameraView lView;

        if (!TryResolveCameraView(lView))
        {
            // Say why there is no picture (a stale image would look like a frozen camera).
            ImGui::TextDisabled("No camera");
            ImGui::TextDisabled("Select an entity with a CameraComponent.");
            ImGui::TextDisabled("It stays on that camera until you pick another.");

            return;
        }

        ImguiWidgets::Image(GetPreviewImage(), lAvail);
    }

    void CameraPreviewPanel::Shutdown()
    {
        // Nothing to unregister: the view is submitted per frame.
        m_RenderTarget.reset();
        m_Framebuffer.reset();
    }
}
