#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // TUniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"    // EntityID — which camera is being previewed

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
    //   THE PREVIEW IS STICKY. Selecting a camera points it there; selecting anything else leaves it
    //   where it was. Following the raw selection meant every click on ordinary geometry blanked the
    //   picture — and since you select geometry to POSITION IT AGAINST the framing, the preview went
    //   dark exactly when it was being used.
    //
    //   NO CAMERA IS A FIRST-CLASS STATE, not an error: it is what a panel opened from the Window
    //   menu shows before any camera has been picked, and what remains after the previewed one is
    //   deleted, loses its component, or belongs to a world that is no longer active.
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
         * Point the preview at the primary selection — ONLY when that selection is a camera.
         *
         * The whole of the sticky rule, and the reason it is its own step: anything else is not a
         * new subject, it is the author working on the scene the preview is there to frame.
         *
         * Runs even while hidden, so opening the panel shows the camera already selected rather
         * than an empty state that needs a re-click.
         */
        void TrackSelection();

        /**
         * How the PREVIEWED entity frames the world, if it still frames it at all.
         *
         * Re-resolved from the id every frame rather than cached, so moving the camera or editing
         * its OrthoSize lands the same frame — and so an entity that has been deleted or has lost
         * its component answers false instead of a stale picture.
         *
         * @return False for every ordinary "nothing to preview": no world, nothing ever picked, or
         *   a previewed entity that is gone, no longer a camera, or not in the world being drawn.
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

        /**
         * Forget the previewed camera: an EntityID means nothing in another world, and entt reuses
         * handles — so a kept id could silently resolve to a DIFFERENT entity that happens to have
         * a camera.
         *
         * IT RARELY SHOWS, and that is not luck: EditorService retargets the SELECTION by Guid
         * before notifying panels (WM3 — a clone preserves entity Guids), so a previewed camera
         * that is ALSO selected is re-tracked on the very next frame and the preview survives Play
         * and Stop. It falls back to "No camera" only when the preview was sticky on a camera that
         * was not the current selection.
         */
        void OnActiveWorldChanged(World* InOld, World* InNew) override;

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

        // WHICH camera is being previewed — the sticky half. Only TrackSelection writes it, and only
        // ever with an entity of the ACTIVE world, which is what lets the resolve rebuild a handle
        // from it without carrying a World* that could dangle.
        EntityID m_Previewed = ENTITY_NONE;

        // One line the first time a camera is actually previewed. A smoke run never selects an
        // entity, so without it "nothing was selected" and "the resolve is broken" read the same.
        bool m_bPreviewLogged = false;
    };
}
