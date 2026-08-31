// Suite: DebugDraw (Renderer/DebugDraw.h) — the engine's per-frame debug line queue (Editor.md D10).
//
// Two halves, both GPU-free by design and therefore fully testable here: the QUEUE (DrawLine /
// DrawBox / Clear — plain data, no GL) and the GEOMETRY (ToQuad — the line -> thin-rotated-quad
// conversion that lets DebugDraw render through the existing Renderer2D::DrawQuad without a new RHI
// primitive). Same "the interesting part needs no context" approach as Renderer/RenderTargetTests.cpp.
#include <doctest.h>

#include "Renderer/DebugDraw.h"
#include "Renderer/Renderer2D.h"   // MakeOutlineInnerHalf — the border maths, GPU-free

#include <cmath>

using namespace Opaax;

namespace
{
    constexpr float kEps = 1e-4f;
    constexpr float kPi  = 3.14159265358979323846f;
}

// =============================================================================
// Queue
// =============================================================================

TEST_CASE("DebugDraw: a fresh queue is empty")
{
    const DebugDraw lDebug;

    CHECK(lDebug.IsEmpty());
    CHECK(lDebug.GetLines().empty());
}

TEST_CASE("DebugDraw: DrawLine appends one entry carrying exactly what was submitted")
{
    DebugDraw lDebug;
    lDebug.DrawLine({ 1.f, 2.f }, { 3.f, 4.f }, { 0.2f, 0.4f, 0.6f, 0.8f }, 2.5f);

    REQUIRE(lDebug.GetLines().size() == 1u);
    CHECK_FALSE(lDebug.IsEmpty());

    const DebugLine& lLine = lDebug.GetLines()[0];
    CHECK(lLine.Start.x     == doctest::Approx(1.f));
    CHECK(lLine.Start.y     == doctest::Approx(2.f));
    CHECK(lLine.End.x       == doctest::Approx(3.f));
    CHECK(lLine.End.y       == doctest::Approx(4.f));
    CHECK(lLine.Color.b     == doctest::Approx(0.6f));
    CHECK(lLine.Thickness   == doctest::Approx(2.5f));
}

TEST_CASE("DebugDraw: submission order is preserved (the renderer draws them in order)")
{
    DebugDraw lDebug;
    lDebug.DrawLine({ 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 0.f, 0.f, 1.f });
    lDebug.DrawLine({ 0.f, 0.f }, { 2.f, 0.f }, { 0.f, 1.f, 0.f, 1.f });

    REQUIRE(lDebug.GetLines().size() == 2u);
    CHECK(lDebug.GetLines()[0].End.x == doctest::Approx(1.f));
    CHECK(lDebug.GetLines()[1].End.x == doctest::Approx(2.f));
}

TEST_CASE("DebugDraw: DrawBox covers the rectangle it was given, corner to corner")
{
    // This used to assert FOUR segments forming a closed loop. A box is one hollow quad now, so the
    // loop is gone — but the thing that loop actually proved, that the outline lands on the right
    // rectangle, still has to hold.
    DebugDraw lDebug;
    lDebug.DrawBox({ 10.f, 20.f }, { 4.f, 6.f }, { 1.f, 1.f, 1.f, 1.f }, 1.f);

    REQUIRE(lDebug.GetBoxes().size() == 1u);

    // Centre (10,20), full size (4,6) -> corners x in [8,12], y in [17,23].
    const DebugBox& lBox = lDebug.GetBoxes()[0];
    CHECK(std::fabs((lBox.Center.x - lBox.Size.x * 0.5f) - 8.f)  < kEps);
    CHECK(std::fabs((lBox.Center.x + lBox.Size.x * 0.5f) - 12.f) < kEps);
    CHECK(std::fabs((lBox.Center.y - lBox.Size.y * 0.5f) - 17.f) < kEps);
    CHECK(std::fabs((lBox.Center.y + lBox.Size.y * 0.5f) - 23.f) < kEps);
}

TEST_CASE("DebugDraw: Clear drops the queue (per-frame contract — nothing survives a frame)")
{
    DebugDraw lDebug;
    lDebug.DrawBox({ 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
    REQUIRE(lDebug.GetBoxes().size() == 1u);

    lDebug.Clear();

    CHECK(lDebug.IsEmpty());
    CHECK(lDebug.GetLines().empty());
    CHECK(lDebug.GetBoxes().empty());

    // Reusable after a drain — the renderer clears every frame and producers refill it.
    lDebug.DrawLine({ 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
    CHECK(lDebug.GetLines().size() == 1u);
}

// =============================================================================
// Geometry — ToQuad
// =============================================================================

TEST_CASE("ToQuad: a horizontal segment becomes an unrotated quad of {length, thickness}")
{
    const DebugQuad lQuad = ToQuad(DebugLine{ { 0.f, 5.f }, { 10.f, 5.f }, { 1.f, 1.f, 1.f, 1.f }, 2.f });

    CHECK(lQuad.Center.x      == doctest::Approx(5.f));   // midpoint
    CHECK(lQuad.Center.y      == doctest::Approx(5.f));
    CHECK(lQuad.Size.x        == doctest::Approx(10.f));  // length
    CHECK(lQuad.Size.y        == doctest::Approx(2.f));   // thickness
    CHECK(lQuad.RotationRad   == doctest::Approx(0.f));
}

TEST_CASE("ToQuad: a vertical segment is the same quad rotated a quarter turn")
{
    const DebugQuad lQuad = ToQuad(DebugLine{ { 3.f, 0.f }, { 3.f, 8.f }, { 1.f, 1.f, 1.f, 1.f }, 1.5f });

    CHECK(lQuad.Center.x    == doctest::Approx(3.f));
    CHECK(lQuad.Center.y    == doctest::Approx(4.f));
    CHECK(lQuad.Size.x      == doctest::Approx(8.f));
    CHECK(lQuad.Size.y      == doctest::Approx(1.5f));
    CHECK(lQuad.RotationRad == doctest::Approx(kPi * 0.5f));
}

TEST_CASE("ToQuad: direction is signed — reversing the endpoints flips the rotation by pi")
{
    const DebugQuad lForward = ToQuad(DebugLine{ { 0.f, 0.f }, { 4.f, 0.f }, {}, 1.f });
    const DebugQuad lReverse = ToQuad(DebugLine{ { 4.f, 0.f }, { 0.f, 0.f }, {}, 1.f });

    // Same covered pixels either way (a quad is symmetric), but the maths must stay consistent.
    CHECK(lForward.Center.x == doctest::Approx(lReverse.Center.x));
    CHECK(lForward.Size.x   == doctest::Approx(lReverse.Size.x));
    CHECK(std::fabs(lReverse.RotationRad) == doctest::Approx(kPi));
}

TEST_CASE("ToQuad: a diagonal segment gets its true length, not its bounding box")
{
    const DebugQuad lQuad = ToQuad(DebugLine{ { 0.f, 0.f }, { 3.f, 4.f }, {}, 1.f });

    CHECK(lQuad.Size.x      == doctest::Approx(5.f));            // 3-4-5, not 3 or 4
    CHECK(lQuad.RotationRad == doctest::Approx(std::atan2(4.f, 3.f)));
}

TEST_CASE("ToQuad: a degenerate zero-length segment is invisible, never NaN")
{
    // atan2(0,0) is defined as 0, so this must not poison the render batch with a NaN transform —
    // it collapses to a zero-width quad that draws nothing.
    const DebugQuad lQuad = ToQuad(DebugLine{ { 7.f, 7.f }, { 7.f, 7.f }, {}, 3.f });

    CHECK(lQuad.Center.x == doctest::Approx(7.f));
    CHECK(lQuad.Size.x   == doctest::Approx(0.f));
    CHECK(lQuad.Size.y   == doctest::Approx(3.f));
    CHECK(lQuad.RotationRad == doctest::Approx(0.f));
    CHECK_FALSE(std::isnan(lQuad.RotationRad));
}

// =============================================================================
// The draw BAND (③b)
// =============================================================================
TEST_CASE("DebugDraw: a segment defaults to the Debug band, above world geometry")
{
    // Every caller written before ③b relies on this default. If it ever changed, selection
    // outlines and entity icons would slide behind the sprites they annotate.
    DebugDraw lDraw;
    lDraw.DrawLine({ 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f });

    REQUIRE(lDraw.GetLines().size() == 1u);
    CHECK(lDraw.GetLines()[0].Layer == ERenderLayer::Debug);
}

TEST_CASE("DebugDraw: a segment can name a band BELOW world geometry")
{
    // What the snap grid needs: Background sorts under Default, so the grid is drawn on rather than
    // over. A grid above every sprite is not a grid, it is a cage.
    DebugDraw lDraw;
    lDraw.DrawLine({ 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f, 1.f, 1.f }, 1.f, ERenderLayer::Background);

    REQUIRE(lDraw.GetLines().size() == 1u);
    CHECK(lDraw.GetLines()[0].Layer == ERenderLayer::Background);
    CHECK(static_cast<int>(ERenderLayer::Background) < static_cast<int>(ERenderLayer::Default));
}

TEST_CASE("DebugDraw: a box carries the band it was given")
{
    // ③b's grid needs Background; everything else defaults to Debug and draws above the world. A
    // box that ignored its band would sit on the wrong side of the sprites.
    DebugDraw lDraw;
    lDraw.DrawBox({ 0.f, 0.f }, { 10.f, 10.f }, { 1.f, 1.f, 1.f, 1.f }, 1.f, ERenderLayer::Background);

    REQUIRE(lDraw.GetBoxes().size() == 1u);
    CHECK(lDraw.GetBoxes()[0].Layer == ERenderLayer::Background);

    lDraw.DrawBounds(Bounds2D::FromCenterSize({ 0.f, 0.f }, { 2.f, 2.f }), { 1.f, 1.f, 1.f, 1.f });
    CHECK(lDraw.GetBoxes()[1].Layer == ERenderLayer::Debug);   // the default
}

// =============================================================================
// Boxes — one hollow quad, not four lines
// =============================================================================

TEST_CASE("DebugDraw: DrawBox queues ONE box and no lines")
{
    // The whole point of the change: a selection of N entities costs N quads, not 4N.
    DebugDraw lDebug;
    lDebug.DrawBox({ 0.f, 0.f }, { 10.f, 20.f }, { 1.f, 0.f, 0.f, 1.f }, 2.f);

    CHECK(lDebug.GetLines().empty());
    REQUIRE(lDebug.GetBoxes().size() == 1u);

    const DebugBox& lBox = lDebug.GetBoxes()[0];
    CHECK(lBox.Center.x  == doctest::Approx(0.f));
    CHECK(lBox.Size.x    == doctest::Approx(10.f));
    CHECK(lBox.Size.y    == doctest::Approx(20.f));
    CHECK(lBox.Thickness == doctest::Approx(2.f));
    CHECK(lBox.Layer     == ERenderLayer::Debug);
}

TEST_CASE("DebugDraw: DrawBounds is DrawBox taking the shape callers already hold")
{
    DebugDraw lDebug;
    lDebug.DrawBounds(Bounds2D::FromCenterSize({ 5.f, 6.f }, { 8.f, 4.f }), { 0.f, 1.f, 0.f, 1.f }, 3.f);

    REQUIRE(lDebug.GetBoxes().size() == 1u);

    // Bounds2D is centre + HALF extent; the box wants the FULL size. Getting that conversion
    // backwards would draw a box at half scale, which reads as "the outline is slightly off".
    const DebugBox& lBox = lDebug.GetBoxes()[0];
    CHECK(lBox.Center.x == doctest::Approx(5.f));
    CHECK(lBox.Center.y == doctest::Approx(6.f));
    CHECK(lBox.Size.x   == doctest::Approx(8.f));
    CHECK(lBox.Size.y   == doctest::Approx(4.f));
}

TEST_CASE("DebugDraw: Clear empties BOTH queues, and IsEmpty accounts for boxes")
{
    DebugDraw lDebug;
    lDebug.DrawBox({ 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });

    CHECK_FALSE(lDebug.IsEmpty());   // a box alone is not an empty queue

    lDebug.DrawLine({ 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
    lDebug.Clear();

    CHECK(lDebug.IsEmpty());
    CHECK(lDebug.GetLines().empty());
    CHECK(lDebug.GetBoxes().empty());
}

// =============================================================================
// MakeOutlineInnerHalf — the border maths, GPU-free
// =============================================================================

TEST_CASE("MakeOutlineInnerHalf: a border eats its thickness off each edge")
{
    // 100 wide, 10 thick: the hole spans 80 of the 100, so its half-extent is 0.4 in local space.
    const Vector2F lInner = MakeOutlineInnerHalf({ 100.f, 100.f }, 10.f);

    CHECK(lInner.x == doctest::Approx(0.4f));
    CHECK(lInner.y == doctest::Approx(0.4f));
}

TEST_CASE("MakeOutlineInnerHalf: each axis is independent, so a non-square box is not skewed")
{
    // Same thickness on a 100x20 box eats a tenth of the width and a half of the height.
    const Vector2F lInner = MakeOutlineInnerHalf({ 100.f, 20.f }, 10.f);

    CHECK(lInner.x == doctest::Approx(0.4f));
    CHECK(lInner.y == doctest::Approx(0.f));   // 0.5 - 10/20 = 0 — the border meets in the middle
}

TEST_CASE("MakeOutlineInnerHalf: the result is CLAMPED to [0, 0.5] at both ends")
{
    // Below 0 is an inside-out hole — the shader's abs() test would pass everywhere and discard the
    // whole quad, so a too-thick border would VANISH instead of drawing solid.
    CHECK(MakeOutlineInnerHalf({ 10.f, 10.f }, 50.f).x == doctest::Approx(0.f));   // thicker than the box
    CHECK(MakeOutlineInnerHalf({ 0.f, 0.f }, 2.f).x    == doctest::Approx(0.f));   // no size at all

    // The OTHER end, which the first version of this got wrong: a zero-width border draws nothing,
    // and a negative one cannot draw less than nothing — both cap at a hole the size of the quad.
    CHECK(MakeOutlineInnerHalf({ 10.f, 10.f }, 0.f).x  == doctest::Approx(0.5f));
    CHECK(MakeOutlineInnerHalf({ 10.f, 10.f }, -1.f).x == doctest::Approx(0.5f));
}
