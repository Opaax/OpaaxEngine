#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class World;

    OPAAX_LOG_CATEGORY(ViewportGestures);
}

namespace Opaax::Editor
{
    class EditorCamera;
    class EditorSelection;

    // =============================================================================
    // ViewportGestures — what the mouse does on an image of a world. Measured in the ImGui pass and
    //   applied in OnPreRender (a draw pass only reads the world). Used by the level viewport and the
    //   prefab panel. Read from ImGui because Edit worlds get no InputManager input. The gate is the
    //   image's hover, not io.WantCaptureMouse or the window's.
    // =============================================================================

    /** Middle-drag pan, wrapped at the image edge, and wheel zoom anchored at the cursor. */
    class CameraGesture
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Reads this frame's pan drag and wheel while the panel's window is current.
         * @param bInHovered Hover of the image, minus anything drawn over it
         * @param InOrigin Top-left of the image in screen pixels (passed, since GetItemRect* would give the
         *   last submitted item)
         * @param InSizePx The image's size
         */
        void Measure(bool bInHovered, const Vector2F& InOrigin, const Vector2F& InSizePx);

        /** Seed, zoom (before the pan, so the zoom anchor stays under the cursor), then pan. */
        void Spend(EditorCamera& InCamera, const Vector2F& InViewportPx);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        Vector2F m_PendingPanPx        = { 0.f, 0.f };   // accumulated screen pixels
        Vector2F m_PendingZoomCursorPx = { 0.f, 0.f };   // viewport-local, the zoom's anchor
        float    m_PendingZoom         = 0.f;            // wheel notches; + zooms in
        bool     m_bPanning            = false;          // the middle button went down over the image
        bool     m_bWrapLogged         = false;
    };

    /**
     * The left button on the image: a click selects at a point; dragging past ImGui's
     * MouseDragThreshold makes a box (drawn as a marquee). Ctrl adds instead of replacing.
     */
    class PickGesture
    {
        // =========================================================================
        // Types
        // =========================================================================
    public:
        enum class EKind : Uint8 { None, Point, Box };

        struct Pick
        {
            EKind    Kind      = EKind::None;
            Vector2F StartPx   = { 0.f, 0.f };   // viewport-local, where the button went down
            Vector2F EndPx     = { 0.f, 0.f };   // viewport-local, where it came up
            bool     bAdditive = false;          // Ctrl was held
        };

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /** Reads the left button while the panel's window is current; call it right after the image. */
        void Measure(bool bInHovered, const Vector2F& InOrigin);

        /** The stored result, cleared first (a refused pick must not retry next frame). */
        Pick Take();

        /**
         * Applies a pick to InSelection, hit-testing InWorld through its own camera view. A point selects
         * the topmost entity, a box everything it overlaps; a plain click on nothing clears.
         * @param InAnchorHalfExtent The icon half-size in world units, so entities that draw nothing are
         *   still clickable (same value the icon is drawn with)
         */
        static void Apply(const Pick& InPick, World& InWorld, EditorSelection& InSelection,
                          const Vector2F& InViewportPx, float InAnchorHalfExtent);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        Pick m_Pending;
        bool m_bSelecting = false;   // the left button is down and started over the image

        // Whether the gesture crossed the drag threshold. Stored, because ImGui::IsMouseDragging is
        // false once the button is released.
        bool m_bWasDrag   = false;
    };
}
