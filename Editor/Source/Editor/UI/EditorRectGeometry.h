#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorRectGeometry — which edge of a rect the cursor is on, and what a drag on that edge does
    //   to it. Two callers with the same arithmetic and different units: the client-drawn WINDOW
    //   frame (Int32 screen pixels) and a sprite sheet FRAME (float texture pixels).
    //
    //   NAMES NO UI BACKEND, deliberately. ImguiLayout.h is the tree's home for pure geometry and
    //   would have been the place, but it includes imgui.h and OpaaxTests reaches editor headers
    //   only when they pull no ImGui. This is the fiddliest logic behind both features and the part
    //   a smoke run physically cannot exercise, so it is worth its own testable header.
    //
    //   Header-only value templates: no OPAAX_API (I6), no state, nothing to export. It was
    //   WindowFrameGeometry.h until the sheet editor needed the identical eight-region hit test —
    //   the names below are the generalisation, and the window's own vocabulary survives as aliases
    //   at the bottom so the title bar reads exactly as it did.
    // =============================================================================

    enum class ERectEdge : Uint8
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

    /**
     * A rect as an origin and an extent.
     *
     * SIGNED origin, whatever T is: a monitor left of the primary gives a negative X, and a frame
     * being dragged past the left edge of its texture goes negative before it is clamped back.
     */
    template<typename T>
    struct TEditorRect
    {
        T X      = T{};
        T Y      = T{};
        T Width  = T{};
        T Height = T{};
    };

    /**
     * Which edge InCursor sits on, within InThickness of InRect's border.
     *
     * CORNERS WIN over edges: a cursor in the top-left square answers TopLeft, never Top. A rect
     * narrower than twice the thickness is all corner, which is the harmless end of that rule.
     *
     * @return None when the cursor is outside the rect, or inside its interior.
     */
    template<typename T>
    constexpr ERectEdge HitTestRect(const TEditorRect<T>& InRect, const T InCursorX,
                                    const T InCursorY, const T InThickness) noexcept
    {
        const bool lInside = InCursorX >= InRect.X && InCursorX < InRect.X + InRect.Width
                          && InCursorY >= InRect.Y && InCursorY < InRect.Y + InRect.Height;

        if (!lInside) { return ERectEdge::None; }

        const bool lLeft   = InCursorX <  InRect.X + InThickness;
        const bool lRight  = InCursorX >= InRect.X + InRect.Width  - InThickness;
        const bool lTop    = InCursorY <  InRect.Y + InThickness;
        const bool lBottom = InCursorY >= InRect.Y + InRect.Height - InThickness;

        if (lTop    && lLeft)  { return ERectEdge::TopLeft; }
        if (lTop    && lRight) { return ERectEdge::TopRight; }
        if (lBottom && lLeft)  { return ERectEdge::BottomLeft; }
        if (lBottom && lRight) { return ERectEdge::BottomRight; }

        if (lLeft)   { return ERectEdge::Left; }
        if (lRight)  { return ERectEdge::Right; }
        if (lTop)    { return ERectEdge::Top; }
        if (lBottom) { return ERectEdge::Bottom; }

        return ERectEdge::None;
    }

    /**
     * InRect after dragging InEdge by (InDeltaX, InDeltaY), never smaller than the minimum.
     *
     * A left or top drag MOVES the origin as well as sizing, so the clamp has to give back what
     * the minimum refused — otherwise the origin keeps walking while the size stands still and the
     * rect slides out from under the cursor.
     */
    template<typename T>
    constexpr TEditorRect<T> ResizeRect(TEditorRect<T> InRect, const ERectEdge InEdge,
                                        T InDeltaX, T InDeltaY,
                                        const T InMinWidth, const T InMinHeight) noexcept
    {
        const bool lLeft   = InEdge == ERectEdge::Left
                          || InEdge == ERectEdge::TopLeft
                          || InEdge == ERectEdge::BottomLeft;

        const bool lRight  = InEdge == ERectEdge::Right
                          || InEdge == ERectEdge::TopRight
                          || InEdge == ERectEdge::BottomRight;

        const bool lTop    = InEdge == ERectEdge::Top
                          || InEdge == ERectEdge::TopLeft
                          || InEdge == ERectEdge::TopRight;

        const bool lBottom = InEdge == ERectEdge::Bottom
                          || InEdge == ERectEdge::BottomLeft
                          || InEdge == ERectEdge::BottomRight;

        if (lLeft)
        {
            T lNewWidth = InRect.Width - InDeltaX;

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
            const T lNewWidth = InRect.Width + InDeltaX;
            InRect.Width = lNewWidth < InMinWidth ? InMinWidth : lNewWidth;
        }

        if (lTop)
        {
            T lNewHeight = InRect.Height - InDeltaY;

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
            const T lNewHeight = InRect.Height + InDeltaY;
            InRect.Height = lNewHeight < InMinHeight ? InMinHeight : lNewHeight;
        }

        return InRect;
    }

    /**
     * InRect pushed and, if it has to be, shrunk to fit inside InWidth x InHeight from the origin.
     *
     * MOVE FIRST, SHRINK ONLY IF IT STILL DOES NOT FIT — dragging a frame off the left edge of its
     * texture should slide it back, not silently make it narrower. A rect larger than the bounds is
     * clamped to them, which is the only remaining answer.
     *
     * The window frame has no use for this (a window may hang off a monitor); a sheet frame must
     * never name pixels the texture does not have.
     */
    template<typename T>
    constexpr TEditorRect<T> ClampRectInside(TEditorRect<T> InRect, const T InWidth, const T InHeight) noexcept
    {
        if (InRect.Width  > InWidth)  { InRect.Width  = InWidth; }
        if (InRect.Height > InHeight) { InRect.Height = InHeight; }

        if (InRect.X < T{})                        { InRect.X = T{}; }
        if (InRect.Y < T{})                        { InRect.Y = T{}; }
        if (InRect.X + InRect.Width  > InWidth)    { InRect.X = InWidth  - InRect.Width; }
        if (InRect.Y + InRect.Height > InHeight)   { InRect.Y = InHeight - InRect.Height; }

        return InRect;
    }

    // =============================================================================
    // The window frame's own vocabulary, unchanged. ImGuiTitleBar and its tests were written
    // against these names and there is no reason for them to learn a generic one.
    // =============================================================================
    using WindowFrameRect  = TEditorRect<Int32>;
    using EWindowFrameEdge = ERectEdge;

    constexpr EWindowFrameEdge HitTestFrame(const WindowFrameRect& InRect, const Int32 InCursorX,
                                            const Int32 InCursorY, const Int32 InThickness) noexcept
    {
        return HitTestRect(InRect, InCursorX, InCursorY, InThickness);
    }

    constexpr WindowFrameRect ResizeFrame(const WindowFrameRect& InRect, const EWindowFrameEdge InEdge,
                                          const Int32 InDeltaX, const Int32 InDeltaY,
                                          const Int32 InMinWidth, const Int32 InMinHeight) noexcept
    {
        return ResizeRect(InRect, InEdge, InDeltaX, InDeltaY, InMinWidth, InMinHeight);
    }
}
