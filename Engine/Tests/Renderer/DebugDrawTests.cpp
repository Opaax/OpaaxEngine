// Suite: DebugDraw (Renderer/DebugDraw.h) — the engine's per-frame debug line queue (Editor.md D10).
//
// Two halves, both GPU-free by design and therefore fully testable here: the QUEUE (DrawLine /
// DrawBox / Clear — plain data, no GL) and the GEOMETRY (ToQuad — the line -> thin-rotated-quad
// conversion that lets DebugDraw render through the existing Renderer2D::DrawQuad without a new RHI
// primitive). Same "the interesting part needs no context" approach as Renderer/RenderTargetTests.cpp.
#include <doctest.h>

#include "Renderer/DebugDraw.h"

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

TEST_CASE("DebugDraw: DrawBox emits exactly 4 segments forming a CLOSED rectangle")
{
    DebugDraw lDebug;
    lDebug.DrawBox({ 10.f, 20.f }, { 4.f, 6.f }, { 1.f, 1.f, 1.f, 1.f }, 1.f);

    REQUIRE(lDebug.GetLines().size() == 4u);

    // Centre (10,20), half-extent (2,3) -> corners x in [8,12], y in [17,23].
    for (const DebugLine& lLine : lDebug.GetLines())
    {
        CHECK(std::fabs(std::fabs(lLine.Start.x - 10.f) - 2.f) < kEps);
        CHECK(std::fabs(std::fabs(lLine.Start.y - 20.f) - 3.f) < kEps);
    }

    // Closed loop: every segment starts where the previous one ended, and the last closes onto
    // the first. This is what makes the outline a rectangle rather than four stray sticks.
    const TDynArray<DebugLine>& lLines = lDebug.GetLines();
    for (size_t i = 0; i < lLines.size(); ++i)
    {
        const DebugLine& lNext = lLines[(i + 1) % lLines.size()];
        CHECK(lLines[i].End.x == doctest::Approx(lNext.Start.x));
        CHECK(lLines[i].End.y == doctest::Approx(lNext.Start.y));
    }
}

TEST_CASE("DebugDraw: Clear drops the queue (per-frame contract — nothing survives a frame)")
{
    DebugDraw lDebug;
    lDebug.DrawBox({ 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f });
    REQUIRE(lDebug.GetLines().size() == 4u);

    lDebug.Clear();

    CHECK(lDebug.IsEmpty());
    CHECK(lDebug.GetLines().empty());

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

TEST_CASE("DebugDraw: DrawBox puts ALL FOUR of its segments in the named band")
{
    // The box forwards to DrawLine four times; forgetting the layer on one of them would leave a
    // single edge floating above the others, which reads as a rendering glitch rather than a bug.
    DebugDraw lDraw;
    lDraw.DrawBox({ 0.f, 0.f }, { 10.f, 10.f }, { 1.f, 1.f, 1.f, 1.f }, 1.f, ERenderLayer::Background);

    REQUIRE(lDraw.GetLines().size() == 4u);

    for (const DebugLine& lLine : lDraw.GetLines())
    {
        CHECK(lLine.Layer == ERenderLayer::Background);
    }
}
