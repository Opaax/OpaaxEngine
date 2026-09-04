#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"

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
    // CameraPreviewPanel — what the SELECTED camera sees, rendered by the engine into this panel's
    //   own FBO. Unity's Camera Preview as a dockable panel: while editing you are looking through
    //   the editor camera, and nothing else says what the PLAYER will be framed by.
    //
    //   THE SECOND VIEW. It is what makes multi-view real — the Viewport submits its target and its
    //   framing, this submits another of each, and one frame renders both (F). It is READ-ONLY and
    //   that is the whole reason it is cheap: it reads the camera entity and never writes
    //   World::SetCameraView, so CAM1's one-view-per-world slot is untouched and there is no second
    //   producer to arbitrate.
    //
    //   It draws NO overlays — no grid, no selection outline, no entity icons — because a preview of
    //   the game that is decorated like the editor is not a preview (the rule EnqueueEntityIcons
    //   already follows for Play worlds).
    //
    //   NO CAMERA IS A FIRST-CLASS STATE, not an error: nothing selected, a selection with no
    //   CameraComponent, or a selection belonging to another world all draw "No camera" and submit
    //   no pass.
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
         * How the SELECTED entity frames the world, if it frames it at all.
         *
         * The PRIMARY selection — the same entity the Inspector draws, so the panel and the button
         * that opens it cannot disagree about which camera is meant.
         *
         * @return False for every ordinary "nothing to preview": no world, nothing selected, a
         *   selection without a camera, or one belonging to a world that is not the one being drawn
         *   (an Edit-world selection during PIE would frame the clone with a stranger's position).
         */
        bool TryResolveCameraView(CameraView& OutView) const;

        /**
         * Claim a pass in this frame for the previewed camera, if there is one and this panel is on
         * screen to show it. Re-submitted every frame (F4's idiom), like the Viewport's.
         */
        void SubmitView();

        /** Resize the FBO to the size DrawContents measured last frame — ViewportPanel's deferred shape. */
        void ApplyPendingResize();

        /** Sample this panel's FBO as an ImGui image, upright. */
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

        /** Zero padding: the preview fills the window edge to edge, as the Viewport's does. */
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

        // One line the first time a camera is actually previewed. A smoke run never selects an
        // entity, so without it "nothing was selected" and "the resolve is broken" read the same.
        bool m_bPreviewLogged = false;
    };
}
