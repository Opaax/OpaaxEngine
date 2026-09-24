#pragma once

#include "Application/Services/ILogger.h"
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
    // ViewportGestures — what the mouse means on an image of a world, MEASURED in the ImGui pass
    //   and SPENT in OnPreRender (**SEL3**): a panel's draw reads the world, anything that writes
    //   it runs outside the pass.
    //
    //   Owned by every panel that shows a world — the level viewport and the prefab panel — so
    //   the two cannot drift (⑦-C P8). ImGui is the SOURCE, not a workaround: an Edit world leaves
    //   the input route closed, so InputManager never sees a button (**IN8**). The gate is the
    //   IMAGE's hover, never io.WantCaptureMouse (**SEL8**) and never the window's.
    // =============================================================================

    /** Middle-drag pan, wrapped at the image edge, and wheel zoom anchored at the cursor. */
    class CameraGesture
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Read this frame's pan drag and wheel while the panel's window is current.
         * @param bInHovered Hover of the IMAGE, with anything drawn over it already subtracted.
         * @param InOrigin Top-left of the image in SCREEN pixels — passed, not read from
         *   GetItemRect*, which names whatever was submitted last.
         * @param InSizePx The image's size, for the same reason.
         */
        void Measure(bool bInHovered, const Vector2F& InOrigin, const Vector2F& InSizePx);

        /** Seed, zoom (before the pan, so the anchor is not moved out from under the cursor), pan. */
        void Spend(EditorCamera& InCamera, const Vector2F& InViewportPx);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        Vector2F m_PendingPanPx        = { 0.f, 0.f };   // accumulated screen pixels
        Vector2F m_PendingZoomCursorPx = { 0.f, 0.f };   // viewport-local, the zoom's anchor
        float    m_PendingZoom         = 0.f;            // wheel notches; + is zoom IN
        bool     m_bPanning            = false;          // the middle button went down over the image
        bool     m_bWrapLogged         = false;
    };

    /**
     * The left button on the image: ONE gesture with two outcomes. The press banks a point;
     * crossing ImGui's own MouseDragThreshold promotes it to a box, painted as a marquee while
     * live. Ctrl adds rather than replaces.
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
        /** Read the left button while the panel's window is current; call it right after the image. */
        void Measure(bool bInHovered, const Vector2F& InOrigin);

        /** The banked outcome, cleared FIRST — a refused pick must not retry next frame. */
        Pick Take();

        /**
         * Spend a pick into InSelection, hit-testing InWorld as it was FRAMED (**SEL2**) — the
         * world's own view, so a PIE clone needs no fork. A point selects the topmost entity, a box
         * everything it overlaps; a plain click on nothing clears.
         * @param InAnchorHalfExtent The icon's half-size in world units, so an entity that draws
         *   nothing is still clickable — the same value the icon is drawn with (**SEL4**).
         */
        static void Apply(const Pick& InPick, World& InWorld, EditorSelection& InSelection,
                          const Vector2F& InViewportPx, float InAnchorHalfExtent);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        Pick m_Pending;
        bool m_bSelecting = false;   // the left button is down and started over the image

        // Whether this gesture ever crossed the drag threshold. REMEMBERED rather than queried at
        // release: ImGui::IsMouseDragging needs the button still down, so it is false exactly on
        // the frame the answer is wanted.
        bool m_bWasDrag   = false;
    };
}
