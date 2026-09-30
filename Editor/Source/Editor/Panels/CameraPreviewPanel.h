#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"    // EntityID

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;
    struct CameraView;

    OPAAX_LOG_CATEGORY(CameraPreviewPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct EditorImage;

    // =============================================================================
    // CameraPreviewPanel — what the selected camera sees, rendered into this panel's own FBO
    //   (like Unity's Camera Preview). A second view: read-only, it never writes World::SetCameraView.
    //   No overlays (grid, outlines, icons): it shows what the game shows.
    //   The preview is sticky: selecting a camera shows it, selecting something else keeps it.
    //   "No camera" is a normal state (nothing picked yet, or the camera is gone).
    // =============================================================================
    class CameraPreviewPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Camera Preview);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit CameraPreviewPanel(EditorContext& InContext);
        ~CameraPreviewPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        CameraPreviewPanel(const CameraPreviewPanel&)            = delete;
        CameraPreviewPanel& operator=(const CameraPreviewPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * Previews the primary selection, only if it is a camera. Runs even while hidden, so opening
         * the panel shows the selected camera.
         */
        void TrackSelection();

        /**
         * The previewed camera's view, resolved every frame (camera edits show at once).
         * @return False when there is nothing to preview: no world, nothing picked, or the entity is
         *   gone, no longer a camera, or not in the active world
         */
        bool TryResolveCameraView(CameraView& OutView) const;

        /**
         * Submits a view for this frame, if there is a camera and the panel is visible.
         */
        void SubmitView();

        /** Resizes the FBO to the size measured last frame. */
        void ApplyPendingResize();

        /** Shows the FBO as an ImGui image. */
        EditorImage GetPreviewImage() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void Startup()      override;
        void OnPreRender()  override;
        void DrawContents() override;
        void Shutdown()     override;

        /**
         * Forgets the previewed camera (an EntityID means nothing in another world). The selection is
         * retargeted by guid first, so a camera that is also selected is found again next frame.
         */
        void OnActiveWorldChanged(World* InOld, World* InNew) override;

        /** No padding: the preview fills the window. */
        PanelWindowStyle GetWindowStyle() const override { return { m_DefaultSize, true }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        TUniquePtr<IFramebuffer>          m_Framebuffer;
        TUniquePtr<OffscreenRenderTarget> m_RenderTarget;

        Vector2F   m_DefaultSize = { 400.f, 260.f };
        Vector2u32 m_Size        = { 1, 1 };
        Vector2u32 m_PendingSize = { 1, 1 };

        // Which camera is previewed. Only TrackSelection writes it, always with an active-world entity.
        EntityID m_Previewed = ENTITY_NONE;

        // Log once, the first time a camera is previewed.
        bool m_bPreviewLogged = false;
    };
}
