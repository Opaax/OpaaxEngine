#include "Editor/Panels/CameraPreviewPanel.h"

#include "Editor/Camera/CameraPreview.hpp"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Panels/EditorPanels.h"     // IsVisible
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"

#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec

#include "Renderer/Camera/CameraComponent.h"
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

        m_Context.Preview.SetCamera(lSelected.GetHandle());
    }

    // =========================================================================
    // GetPreviewedCamera — the previewed entity, if it is still a camera in the active world.
    // =========================================================================
    Entity CameraPreviewPanel::GetPreviewedCamera() const
    {
        World* const   lWorld  = m_Context.Worlds.GetActiveWorld();
        const EntityID lCamera = m_Context.Preview.GetCamera();

        if (lWorld == nullptr || lCamera == ENTITY_NONE)
        {
            return Entity{};
        }

        // In the active world: TrackSelection only stores entities of the active world.
        const Entity lEntity{ lCamera, lWorld };

        if (!lEntity.IsValid() || !lEntity.Has<TransformComponent>() || !lEntity.Has<CameraComponent>())
        {
            return Entity{};
        }

        return lEntity;
    }

    void CameraPreviewPanel::OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/)
    {
        m_Context.Preview.Forget();
    }

    void CameraPreviewPanel::SubmitView()
    {
        // A hidden preview draws nothing and costs no pass.
        const Entity lCamera  = GetPreviewedCamera();
        const bool   bSubmits = m_RenderTarget != nullptr && m_Context.Panels.IsVisible(PanelID()) && lCamera.IsValid();

        m_Context.Preview.MarkSubmitted(bSubmits);
        if (!bSubmits)
        {
            return;
        }

        // Panels run before the frame's steps: the renderer frames the camera when it draws, at the
        // alpha the world's poses are drawn with. No overlays: the preview shows what the game shows.
        m_Context.Engine.SubmitCameraView(*m_RenderTarget, lCamera.GetHandle(), /*bInDrawOverlays*/ false,
                                          m_Context.Preview.DrawnViewSlot(), lCamera.GetWorld());

        if (!m_bPreviewLogged)
        {
            m_bPreviewLogged = true;

            OPAAX_LOG(LogCameraPreviewPanel, Info, "Previewing camera '{}' into a {}x{} target",
                      lCamera.GetName().CStr(), m_Size.x, m_Size.y);
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

        if (!GetPreviewedCamera().IsValid())
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
