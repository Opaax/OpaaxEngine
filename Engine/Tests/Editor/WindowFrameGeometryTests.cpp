// Suite: the client-drawn window frame's arithmetic (Editor/UI/WindowFrameGeometry.h).
//
// The editor draws its own title bar, so it re-implements what the OS used to do: hit-test the
// resize border and turn a drag into a new window rect. A smoke run cannot reach ANY of it — it
// never drags a window edge — so this is where the 8 regions, the corner priority and the
// minimum-size clamp are held.
//
// The header names no UI backend precisely so this file can exist: OpaaxTests reaches editor
// headers only when they pull no ImGui.
#include <doctest.h>

#include "Editor/UI/WindowFrameGeometry.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    // A window at a NEGATIVE origin on purpose: PosX/PosY were Uint32 until this block, and a
    // monitor left of the primary is exactly the case that wrapped.
    constexpr WindowFrameRect k_Rect{-100, -50, 800, 600};
    constexpr Int32           k_Thickness = 6;

    constexpr EWindowFrameEdge Hit(const Int32 InX, const Int32 InY) noexcept
    {
        return HitTestFrame(k_Rect, InX, InY, k_Thickness);
    }
}

TEST_CASE("HitTestFrame: every one of the eight regions answers itself")
{
    // Corners, one pixel inside each.
    CHECK(Hit(-100, -50) == EWindowFrameEdge::TopLeft);
    CHECK(Hit(699, -50) == EWindowFrameEdge::TopRight);
    CHECK(Hit(-100, 549) == EWindowFrameEdge::BottomLeft);
    CHECK(Hit(699, 549) == EWindowFrameEdge::BottomRight);

    // Edges, mid-span so no corner can claim them.
    CHECK(Hit(-100, 250) == EWindowFrameEdge::Left);
    CHECK(Hit(699, 250) == EWindowFrameEdge::Right);
    CHECK(Hit(300, -50) == EWindowFrameEdge::Top);
    CHECK(Hit(300, 549) == EWindowFrameEdge::Bottom);
}

TEST_CASE("HitTestFrame: a corner beats the edges it overlaps")
{
    // Inside the top band AND the left band. Both are true here; TopLeft has to win, or a diagonal
    // drag resizes in one axis only.
    CHECK(Hit(-96, -46) == EWindowFrameEdge::TopLeft);

    // One pixel past the corner square in X is the top EDGE, not the corner.
    CHECK(Hit(-94, -46) == EWindowFrameEdge::Top);
}

TEST_CASE("HitTestFrame: the interior and the outside are both None")
{
    CHECK(Hit(300, 250) == EWindowFrameEdge::None);          // middle of the window
    CHECK(Hit(-94, -44) == EWindowFrameEdge::None);          // just inside both bands
    CHECK(Hit(-101, 250) == EWindowFrameEdge::None);         // one pixel left of the window
    CHECK(Hit(700, 250) == EWindowFrameEdge::None);          // one pixel past the right edge
    CHECK(Hit(300, 550) == EWindowFrameEdge::None);          // one pixel below the bottom edge
}

TEST_CASE("ResizeFrame: a right/bottom drag sizes without moving the origin")
{
    const WindowFrameRect lOut = ResizeFrame(k_Rect, EWindowFrameEdge::BottomRight, 40, 30, 480, 320);

    CHECK(lOut.X == -100);
    CHECK(lOut.Y == -50);
    CHECK(lOut.Width == 840);
    CHECK(lOut.Height == 630);
}

TEST_CASE("ResizeFrame: a left/top drag moves the origin AND sizes")
{
    // Dragging the top-left corner right and down shrinks the window and walks its origin with it.
    const WindowFrameRect lOut = ResizeFrame(k_Rect, EWindowFrameEdge::TopLeft, 40, 30, 480, 320);

    CHECK(lOut.X == -60);
    CHECK(lOut.Y == -20);
    CHECK(lOut.Width == 760);
    CHECK(lOut.Height == 570);
}

TEST_CASE("ResizeFrame: the minimum clamps the SIZE and stops the origin overshooting")
{
    // 800 - 700 = 100, well under the 480 minimum. The width pins at 480, and the origin must only
    // travel the 320 the clamp actually allowed — not the full 700. Letting X take the whole delta
    // is the bug this case exists for: the window would slide out from under the cursor while its
    // size stood still.
    const WindowFrameRect lOut = ResizeFrame(k_Rect, EWindowFrameEdge::Left, 700, 0, 480, 320);

    CHECK(lOut.Width == 480);
    CHECK(lOut.X == -100 + 320);

    // The far edge has not moved, which is the property the clamp is really protecting.
    CHECK(lOut.X + lOut.Width == k_Rect.X + k_Rect.Width);
}

TEST_CASE("ResizeFrame: the top edge clamps the same way, and the bottom edge simply pins")
{
    const WindowFrameRect lTop = ResizeFrame(k_Rect, EWindowFrameEdge::Top, 0, 900, 480, 320);

    CHECK(lTop.Height == 320);
    CHECK(lTop.Y + lTop.Height == k_Rect.Y + k_Rect.Height);

    // A bottom drag never moves the origin, so a clamped one just stops shrinking.
    const WindowFrameRect lBottom = ResizeFrame(k_Rect, EWindowFrameEdge::Bottom, 0, -900, 480, 320);

    CHECK(lBottom.Y == k_Rect.Y);
    CHECK(lBottom.Height == 320);
}

TEST_CASE("ResizeFrame: None is a no-op in both axes")
{
    const WindowFrameRect lOut = ResizeFrame(k_Rect, EWindowFrameEdge::None, 40, 30, 480, 320);

    CHECK(lOut.X == k_Rect.X);
    CHECK(lOut.Y == k_Rect.Y);
    CHECK(lOut.Width == k_Rect.Width);
    CHECK(lOut.Height == k_Rect.Height);
}
