#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // WindowFrameGeometry — the arithmetic behind a client-drawn window frame: which edge the
    //   cursor is on, and what a drag on that edge does to the window rect.
    //
    //   NAMES NO UI BACKEND, deliberately. ImguiLayout.h is the tree's home for pure geometry and
    //   would have been the place, but it includes imgui.h and OpaaxTests reaches editor headers
    //   only when they pull no ImGui. This is the fiddliest logic behind the title bar and the
    //   part a smoke run physically cannot exercise, so it is worth its own testable header.
    // =============================================================================

    enum class EWindowFrameEdge : Uint8
    {
        None = 0,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    /** A window rect in pixels. SIGNED origin — a monitor left of the primary gives a negative X. */
    struct WindowFrameRect
    {
        Int32 X      = 0;
        Int32 Y      = 0;
        Int32 Width  = 0;
        Int32 Height = 0;
    };

    /**
     * Which edge InCursor sits on, within InThickness of InRect's border.
     *
     * CORNERS WIN over edges: a cursor in the top-left square answers TopLeft, never Top. A window
     * narrower than twice the thickness is all corner, which is the harmless end of that rule.
     *
     * @return None when the cursor is outside the rect, or inside its interior.
     */
    constexpr EWindowFrameEdge HitTestFrame(const WindowFrameRect& InRect, const Int32 InCursorX,
                                            const Int32 InCursorY, const Int32 InThickness) noexcept
    {
        const bool lInside = InCursorX >= InRect.X && InCursorX < InRect.X + InRect.Width
                          && InCursorY >= InRect.Y && InCursorY < InRect.Y + InRect.Height;

        if (!lInside) { return EWindowFrameEdge::None; }

        const bool lLeft   = InCursorX <  InRect.X + InThickness;
        const bool lRight  = InCursorX >= InRect.X + InRect.Width  - InThickness;
        const bool lTop    = InCursorY <  InRect.Y + InThickness;
        const bool lBottom = InCursorY >= InRect.Y + InRect.Height - InThickness;

        if (lTop    && lLeft)  { return EWindowFrameEdge::TopLeft; }
        if (lTop    && lRight) { return EWindowFrameEdge::TopRight; }
        if (lBottom && lLeft)  { return EWindowFrameEdge::BottomLeft; }
        if (lBottom && lRight) { return EWindowFrameEdge::BottomRight; }

        if (lLeft)   { return EWindowFrameEdge::Left; }
        if (lRight)  { return EWindowFrameEdge::Right; }
        if (lTop)    { return EWindowFrameEdge::Top; }
        if (lBottom) { return EWindowFrameEdge::Bottom; }

        return EWindowFrameEdge::None;
    }

    /**
     * InRect after dragging InEdge by (InDeltaX, InDeltaY), never smaller than the minimum.
     *
     * A left or top drag MOVES the origin as well as sizing, so the clamp has to give back what
     * the minimum refused — otherwise the origin keeps walking while the size stands still and the
     * window slides out from under the cursor.
     */
    constexpr WindowFrameRect ResizeFrame(WindowFrameRect InRect, const EWindowFrameEdge InEdge,
                                          Int32 InDeltaX, Int32 InDeltaY,
                                          const Int32 InMinWidth, const Int32 InMinHeight) noexcept
    {
        const bool lLeft   = InEdge == EWindowFrameEdge::Left
                          || InEdge == EWindowFrameEdge::TopLeft
                          || InEdge == EWindowFrameEdge::BottomLeft;

        const bool lRight  = InEdge == EWindowFrameEdge::Right
                          || InEdge == EWindowFrameEdge::TopRight
                          || InEdge == EWindowFrameEdge::BottomRight;

        const bool lTop    = InEdge == EWindowFrameEdge::Top
                          || InEdge == EWindowFrameEdge::TopLeft
                          || InEdge == EWindowFrameEdge::TopRight;

        const bool lBottom = InEdge == EWindowFrameEdge::Bottom
                          || InEdge == EWindowFrameEdge::BottomLeft
                          || InEdge == EWindowFrameEdge::BottomRight;

        if (lLeft)
        {
            Int32 lNewWidth = InRect.Width - InDeltaX;

            if (lNewWidth < InMinWidth)
            {
                InDeltaX -= InMinWidth - lNewWidth;
                lNewWidth = InMinWidth;
            }

            InRect.X    += InDeltaX;
            InRect.Width = lNewWidth;
        }
        else if (lRight)
        {
            const Int32 lNewWidth = InRect.Width + InDeltaX;
            InRect.Width = lNewWidth < InMinWidth ? InMinWidth : lNewWidth;
        }

        if (lTop)
        {
            Int32 lNewHeight = InRect.Height - InDeltaY;

            if (lNewHeight < InMinHeight)
            {
                InDeltaY -= InMinHeight - lNewHeight;
                lNewHeight = InMinHeight;
            }

            InRect.Y     += InDeltaY;
            InRect.Height = lNewHeight;
        }
        else if (lBottom)
        {
            const Int32 lNewHeight = InRect.Height + InDeltaY;
            InRect.Height = lNewHeight < InMinHeight ? InMinHeight : lNewHeight;
        }

        return InRect;
    }
}
