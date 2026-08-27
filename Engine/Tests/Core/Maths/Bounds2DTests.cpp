// Suite: the axis-aligned box (Core/Maths/Bounds2D.h).
//
// Pure geometry, so all of it is testable with no world and no GL context — which matters because
// it is what every hit test in the editor stands on. The rotated cover and the any-direction
// FromMinMax are the two that would fail silently: a turned sprite would simply stop being
// clickable near its corners, and a drag made right-to-left would select nothing.
#include <doctest.h>

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathsStatics.h"

using namespace Opaax;

TEST_CASE("Bounds2D: FromCenterSize halves the size, and Size gives it back")
{
    const Bounds2D lBounds = Bounds2D::FromCenterSize({ 10.f, -4.f }, { 100.f, 40.f });

    CHECK(lBounds.Center.x == doctest::Approx(10.f));
    CHECK(lBounds.HalfExtent.x == doctest::Approx(50.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(20.f));

    // DebugDraw::DrawBox takes a FULL size, so this round trip is what keeps the drawn outline the
    // same rectangle as the one that was hit-tested.
    CHECK(lBounds.Size().x == doctest::Approx(100.f));
    CHECK(lBounds.Size().y == doctest::Approx(40.f));

    CHECK(lBounds.Min().x == doctest::Approx(-40.f));
    CHECK(lBounds.Max().y == doctest::Approx(16.f));
}

TEST_CASE("Bounds2D: Contains is inclusive on the edge")
{
    const Bounds2D lBounds = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 100.f, 100.f });

    CHECK(lBounds.Contains({ 0.f, 0.f }));
    CHECK(lBounds.Contains({ 49.9f, -49.9f }));

    // Clicking a sprite's exact border must hit it — a half-open box loses the top and right edges.
    CHECK(lBounds.Contains({ 50.f, 50.f }));
    CHECK(lBounds.Contains({ -50.f, 0.f }));

    CHECK_FALSE(lBounds.Contains({ 50.1f, 0.f }));
    CHECK_FALSE(lBounds.Contains({ 0.f, -50.1f }));
}

TEST_CASE("Bounds2D: Intersects counts touching, and separates on either axis alone")
{
    const Bounds2D lA = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 100.f, 100.f });

    CHECK(lA.Intersects(lA));
    CHECK(lA.Intersects(Bounds2D::FromCenterSize({ 90.f, 0.f }, { 100.f, 100.f })));   // overlapping
    CHECK(lA.Intersects(Bounds2D::FromCenterSize({ 100.f, 0.f }, { 100.f, 100.f })));  // edge to edge

    // Apart on X only, and on Y only — a marquee that ANDs the two axes wrongly passes one of these.
    CHECK_FALSE(lA.Intersects(Bounds2D::FromCenterSize({ 101.f, 0.f }, { 100.f, 100.f })));
    CHECK_FALSE(lA.Intersects(Bounds2D::FromCenterSize({ 0.f, 101.f }, { 100.f, 100.f })));

    // Fully contained is still an intersection — a marquee dragged inside a big sprite selects it.
    CHECK(lA.Intersects(Bounds2D::FromCenterSize({ 0.f, 0.f }, { 10.f, 10.f })));
}

TEST_CASE("Bounds2D: FromMinMax normalises corners given in ANY order")
{
    // The four drag directions. A marquee dragged up-left produces the reversed pair, and an
    // implementation that subtracts blindly yields a NEGATIVE half-extent, which Contains then
    // reports as empty — the drag would select nothing at all.
    const Bounds2D lExpected = Bounds2D::FromMinMax({ -10.f, -20.f }, { 30.f, 40.f });

    CHECK(lExpected.Center.x == doctest::Approx(10.f));
    CHECK(lExpected.Center.y == doctest::Approx(10.f));
    CHECK(lExpected.HalfExtent.x == doctest::Approx(20.f));
    CHECK(lExpected.HalfExtent.y == doctest::Approx(30.f));

    for (const Bounds2D& lFlipped : { Bounds2D::FromMinMax({ 30.f, 40.f }, { -10.f, -20.f }),
                                      Bounds2D::FromMinMax({ -10.f, 40.f }, { 30.f, -20.f }),
                                      Bounds2D::FromMinMax({ 30.f, -20.f }, { -10.f, 40.f }) })
    {
        CHECK(lFlipped.HalfExtent.x == doctest::Approx(lExpected.HalfExtent.x));
        CHECK(lFlipped.HalfExtent.y == doctest::Approx(lExpected.HalfExtent.y));
        CHECK(lFlipped.Center.x == doctest::Approx(lExpected.Center.x));
        CHECK(lFlipped.Center.y == doctest::Approx(lExpected.Center.y));
    }
}

TEST_CASE("Bounds2D: the rotated cover GROWS with the angle")
{
    const Vector2F lCenter{ 5.f, 5.f };
    const Vector2F lSize{ 100.f, 20.f };

    // Zero is the axis-aligned box exactly — the common case must cost nothing in accuracy.
    const Bounds2D lFlat = Bounds2D::FromCenterSizeRotated(lCenter, lSize, 0.f);
    CHECK(lFlat.HalfExtent.x == doctest::Approx(50.f));
    CHECK(lFlat.HalfExtent.y == doctest::Approx(10.f));

    // A quarter turn swaps the extents; the centre never moves.
    const Bounds2D lQuarter = Bounds2D::FromCenterSizeRotated(lCenter, lSize, FHALF_PI);
    CHECK(lQuarter.HalfExtent.x == doctest::Approx(10.f));
    CHECK(lQuarter.HalfExtent.y == doctest::Approx(50.f));
    CHECK(lQuarter.Center.x == doctest::Approx(5.f));

    // 45 degrees. Note X SHRINKS (50 -> 42.4) while Y grows five-fold — a cover that only ever grew
    // would be wrong too. What has to hold is containment, not monotonic size.
    const Bounds2D lDiagonal = Bounds2D::FromCenterSizeRotated(lCenter, lSize, FQUARTER_PI);
    const float    lHalfDiag = (50.f + 10.f) * 0.70710678f;
    CHECK(lDiagonal.HalfExtent.x == doctest::Approx(lHalfDiag));
    CHECK(lDiagonal.HalfExtent.y == doctest::Approx(lHalfDiag));
    CHECK(lDiagonal.HalfExtent.y > lFlat.HalfExtent.y);

    // THE POINT OF THE WHOLE FUNCTION: the turned box's own corner is inside its cover. Ignoring
    // rotation gives HalfExtent.y == 10 here, so this corner sits four times outside — a rotated
    // sprite would go unclickable across most of its length, and its outline would be visibly wrong.
    const float lC = 0.70710678f;
    const Vector2F lCorner{ lCenter.x + (50.f * lC - 10.f * lC), lCenter.y + (50.f * lC + 10.f * lC) };
    CHECK(lDiagonal.Contains(lCorner));
    CHECK_FALSE(lFlat.Contains(lCorner));

    // Negative angles cover the same area — the formula takes absolutes for exactly this.
    const Bounds2D lNegative = Bounds2D::FromCenterSizeRotated(lCenter, lSize, -FQUARTER_PI);
    CHECK(lNegative.HalfExtent.x == doctest::Approx(lDiagonal.HalfExtent.x));
}

TEST_CASE("Bounds2D: Encapsulate covers both, and is a no-op for a box already inside")
{
    Bounds2D lBounds = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 100.f, 100.f });

    lBounds.Encapsulate(Bounds2D::FromCenterSize({ 200.f, 0.f }, { 100.f, 100.f }));

    // -50 .. 250 on X, unchanged on Y.
    CHECK(lBounds.Min().x == doctest::Approx(-50.f));
    CHECK(lBounds.Max().x == doctest::Approx(250.f));
    CHECK(lBounds.Min().y == doctest::Approx(-50.f));
    CHECK(lBounds.Max().y == doctest::Approx(50.f));

    // Focus on a multi-selection folds one box at a time, so swallowing a contained one must not
    // move the result — otherwise the framing would depend on selection ORDER.
    const Bounds2D lBefore = lBounds;
    lBounds.Encapsulate(Bounds2D::FromCenterSize({ 0.f, 0.f }, { 10.f, 10.f }));

    CHECK(lBounds.Min().x == doctest::Approx(lBefore.Min().x));
    CHECK(lBounds.Max().x == doctest::Approx(lBefore.Max().x));
    CHECK(lBounds.Max().y == doctest::Approx(lBefore.Max().y));
}
