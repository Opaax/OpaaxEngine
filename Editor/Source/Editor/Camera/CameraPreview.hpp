#pragma once

#include "Renderer/CameraView.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // CameraPreview — the camera the Camera Preview panel shows, and the view it was last drawn with.
    //   The panel picks the camera and submits it every frame; the renderer writes the view when it
    //   draws it, after the fixed steps; automation reads both (editor.camera.preview).
    // =============================================================================
    class CameraPreview
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Previews InCamera, an entity of the active world. */
        void SetCamera(const EntityID InCamera) noexcept { m_Camera = InCamera; }

        /** Nothing previewed: an EntityID means nothing in another world. */
        void Forget() noexcept
        {
            m_Camera     = ENTITY_NONE;
            m_bSubmitted = false;
        }

        /** Whether the panel submitted the camera for this frame (open, and a camera to show). */
        void MarkSubmitted(const bool bInSubmitted) noexcept { m_bSubmitted = bInSubmitted; }

        /** Where the renderer writes the view it draws the preview with. */
        CameraView* DrawnViewSlot() noexcept { return &m_DrawnView; }

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        EntityID          GetCamera() const noexcept    { return m_Camera; }
        bool              WasSubmitted() const noexcept { return m_bSubmitted; }
        const CameraView& GetDrawnView() const noexcept { return m_DrawnView; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EntityID   m_Camera     = ENTITY_NONE;
        bool       m_bSubmitted = false;
        CameraView m_DrawnView;
    };
}
