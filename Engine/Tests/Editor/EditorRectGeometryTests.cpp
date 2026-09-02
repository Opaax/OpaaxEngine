// Suite: the rect arithmetic behind BOTH the client-drawn window frame and the sprite sheet
// editor's frame rects (Editor/UI/EditorRectGeometry.h).
//
// The window cases below still speak the window's own vocabulary, which is now an ALIAS of the
// generic one — so they double as the guard that the alias still names the same thing.
//
// The editor draws its own title bar, so it re-implements what the OS used to do: hit-test the
// resize border and turn a drag into a new window rect. A smoke run cannot reach ANY of it — it
// never drags a window edge — so this is where the 8 regions, the corner priority and the
// minimum-size clamp are held.
//
// The header names no UI backend precisely so this file can exist: OpaaxTests reaches editor
// headers only when they pull no ImGui.
#include <doctest.h>

#include "Editor/UI/EditorRectGeometry.h"

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

// =============================================================================
// The FLOAT instantiation — a sprite sheet frame, in texture pixels. Same body, so these cases
// are about the generalisation holding rather than about the arithmetic being re-derived.
// =============================================================================
using SheetRect = Opaax::Editor::TEditorRect<float>;

TEST_CASE("EditorRectGeometry: the eight regions work in float texture pixels")
{
    const SheetRect lFrame{ 32.f, 32.f, 32.f, 32.f };

    // A 4px grab band, which is what a sheet canvas uses at 1:1 zoom.
    CHECK(HitTestRect(lFrame, 33.f, 33.f, 4.f) == ERectEdge::TopLeft);
    CHECK(HitTestRect(lFrame, 62.f, 33.f, 4.f) == ERectEdge::TopRight);
    CHECK(HitTestRect(lFrame, 33.f, 62.f, 4.f) == ERectEdge::BottomLeft);
    CHECK(HitTestRect(lFrame, 48.f, 33.f, 4.f) == ERectEdge::Top);
    CHECK(HitTestRect(lFrame, 48.f, 48.f, 4.f) == ERectEdge::None);   // the interior
    CHECK(HitTestRect(lFrame, 10.f, 10.f, 4.f) == ERectEdge::None);   // outside
}

TEST_CASE("EditorRectGeometry: dragging a frame's left edge moves its origin, and stops at the minimum")
{
    const SheetRect lFrame{ 32.f, 32.f, 32.f, 32.f };

    const SheetRect lWider = ResizeRect(lFrame, ERectEdge::Left, -8.f, 0.f, 1.f, 1.f);
    CHECK(lWider.X     == doctest::Approx(24.f));
    CHECK(lWider.Width == doctest::Approx(40.f));

    // Past the minimum the origin must STOP with the size, or the frame walks out from under the
    // cursor — the same compensation the window frame needs, now proven for float too.
    const SheetRect lPinned = ResizeRect(lFrame, ERectEdge::Left, 100.f, 0.f, 4.f, 4.f);
    CHECK(lPinned.Width == doctest::Approx(4.f));
    CHECK(lPinned.X     == doctest::Approx(60.f));   // 32 + 32 - 4, the right edge held still
}

// =============================================================================
// ClampRectInside — the sheet's own rule: a frame may never name pixels the texture lacks.
// =============================================================================
TEST_CASE("ClampRectInside: a frame dragged off an edge SLIDES BACK, it does not shrink")
{
    // Shrinking here would be data loss disguised as a clamp: the author moved it, they did not
    // resize it.
    const SheetRect lOff = ClampRectInside(SheetRect{ -10.f, -6.f, 32.f, 32.f }, 64.f, 64.f);

    CHECK(lOff.X      == doctest::Approx(0.f));
    CHECK(lOff.Y      == doctest::Approx(0.f));
    CHECK(lOff.Width  == doctest::Approx(32.f));
    CHECK(lOff.Height == doctest::Approx(32.f));

    const SheetRect lPast = ClampRectInside(SheetRect{ 50.f, 40.f, 32.f, 32.f }, 64.f, 64.f);
    CHECK(lPast.X == doctest::Approx(32.f));
    CHECK(lPast.Y == doctest::Approx(32.f));
}

TEST_CASE("ClampRectInside: a frame LARGER than the texture is cut down to it")
{
    const SheetRect lHuge = ClampRectInside(SheetRect{ 10.f, 10.f, 200.f, 90.f }, 64.f, 64.f);

    CHECK(lHuge.X      == doctest::Approx(0.f));
    CHECK(lHuge.Y      == doctest::Approx(0.f));
    CHECK(lHuge.Width  == doctest::Approx(64.f));
    CHECK(lHuge.Height == doctest::Approx(64.f));
}

TEST_CASE("ClampRectInside: a frame already inside is untouched")
{
    const SheetRect lIn = ClampRectInside(SheetRect{ 8.f, 8.f, 16.f, 16.f }, 64.f, 64.f);

    CHECK(lIn.X      == doctest::Approx(8.f));
    CHECK(lIn.Y      == doctest::Approx(8.f));
    CHECK(lIn.Width  == doctest::Approx(16.f));
    CHECK(lIn.Height == doctest::Approx(16.f));
}
