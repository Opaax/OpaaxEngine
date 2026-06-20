#pragma once
#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Renderer/Camera/OrthographicCamera.h"

namespace Opaax::Editor
{
    /**
     * @class EditorCamera
     *
     * Editor-only viewport camera. Inherits OrthographicCamera's matrix machinery and adds
     * a behavior surface for mouse-driven pan + scroll-driven zoom-at-cursor. Lifetime is
     * owned by EditorSubsystem (one instance, survives PIE cycles so pan/zoom state persists
     * across Play → Stop). Installed as the non-owning active camera on RenderSubsystem in
     * Editing state; swapped out for a fresh runtime OrthographicCamera at PIE Start; swapped
     * back at PIE Stop with state intact.
     */
    class OPAAX_API EditorCamera final : public OrthographicCamera
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        EditorCamera()                                   = default;
        ~EditorCamera() override                         = default;

        EditorCamera(const EditorCamera&)                = delete;
        EditorCamera& operator=(const EditorCamera&)     = delete;
        EditorCamera(EditorCamera&&)                     = delete;
        EditorCamera& operator=(EditorCamera&&)          = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Apply a screen-pixel-space mouse-drag delta as a "grab the world" pan. Drag-right
         * slides world content right (camera moves left); Y axis flipped because screen-Y
         * grows downward while world-Y grows upward. Delta is divided by current zoom so
         * one pixel of cursor travel = one world unit at zoom = 1.
         */
        void Pan(const Vector2F& InScreenDelta);

        /**
         * Apply a scroll-wheel zoom anchored at the cursor's viewport-local position. The
         * world point under the cursor stays under the cursor across the zoom change.
         * Zoom factor per scroll click = 1.1; clamped to [0.1, 10.0].
         */
        void Zoom(float InScrollDelta, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx);
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
