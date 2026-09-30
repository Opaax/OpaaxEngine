#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorRectGeometry — which edge of a rect the cursor is on, and what dragging that edge does.
    //   Used by the window frame (Int32 screen pixels) and the sprite sheet frames (float texture
    //   pixels). Header-only and ImGui-free so tests can include it.
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
     * A rect as an origin and an extent. The origin is signed whatever T is (a monitor left of the
     * primary, or a frame dragged past its texture's left edge).
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
     * Which edge InCursor is on, within InThickness of InRect's border. Corners win over edges.
     * @return None when the cursor is outside the rect or in its interior
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
     * InRect after dragging InEdge by (InDeltaX, InDeltaY), never smaller than the minimum. A left or
     * top drag also moves the origin, so the clamp keeps the opposite edge in place.
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
     * InRect moved (and shrunk only if it still does not fit) to lie inside InWidth x InHeight.
     * Used for sheet frames, which must stay inside their texture.
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
    // The window frame's names, kept as aliases.
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
