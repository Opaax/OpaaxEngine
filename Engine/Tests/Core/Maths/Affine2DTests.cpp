// Suite: Affine2D. The transform the UI's poses draw and hit-test with: a point must come back
// through the inverse (or a click lands elsewhere than the drawing), and a pivot must stay put.
#include <doctest.h>

#include "Core/Maths/Affine2D.h"

using namespace Opaax;

namespace
{
    void CheckPoint(const Vector2F& InActual, const Vector2F& InExpected)
    {
        CHECK(InActual.x == doctest::Approx(InExpected.x).epsilon(1e-4));
        CHECK(InActual.y == doctest::Approx(InExpected.y).epsilon(1e-4));
    }
}

TEST_CASE("Affine2D: identity leaves points alone")
{
    const Affine2D lIdentity = Affine2D::Identity();
    CHECK(lIdentity.IsIdentity());
    CheckPoint(lIdentity.TransformPoint({ 3.f, -7.f }), { 3.f, -7.f });

    CHECK_FALSE(Affine2D::FromTranslationRotationScale({ 1.f, 0.f }, 0.f, { 1.f, 1.f }).IsIdentity());
}

TEST_CASE("Affine2D: a turn is counter-clockwise, then the translation")
{
    const Affine2D lTurn = Affine2D::FromTranslationRotationScale({ 10.f, 0.f }, 90.f, { 2.f, 2.f });
    CheckPoint(lTurn.TransformPoint({ 1.f, 0.f }), { 10.f, 2.f });
    CheckPoint(lTurn.TransformPoint({ 0.f, 1.f }), { 8.f, 0.f });
    CheckPoint(lTurn.TransformVector({ 1.f, 0.f }), { 0.f, 2.f });
}

TEST_CASE("Affine2D: a point goes there and back through the inverse")
{
    const Affine2D lPose = Affine2D::FromPose({ 25.f, -4.f }, 37.f, { 1.5f, 0.75f }, { 100.f, 50.f });

    Affine2D lInverse;
    REQUIRE(lPose.Inverse(lInverse));

    const Vector2F lPoints[] = { { 0.f, 0.f }, { 120.f, -30.f }, { -5.f, 400.f } };
    for (const Vector2F& lPoint : lPoints)
    {
        CheckPoint(lInverse.TransformPoint(lPose.TransformPoint(lPoint)), lPoint);
    }

    CHECK((lPose * lInverse).Decompose().Scale.x == doctest::Approx(1.f));
}

TEST_CASE("Affine2D: a scale of 0 has no inverse")
{
    const Affine2D lFlat = Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 0.f, { 0.f, 1.f });

    Affine2D lInverse = Affine2D::FromTranslationRotationScale({ 9.f, 9.f }, 0.f, { 1.f, 1.f });
    CHECK_FALSE(lFlat.Inverse(lInverse));
    CHECK(lInverse.Translation.x == 9.f);
}

TEST_CASE("Affine2D: the pivot stays put under a scale or a turn")
{
    const Vector2F lPivot{ 40.f, 20.f };

    CheckPoint(Affine2D::FromPose({ 0.f, 0.f }, 0.f, { 3.f, 0.5f }, lPivot).TransformPoint(lPivot), lPivot);
    CheckPoint(Affine2D::FromPose({ 0.f, 0.f }, 65.f, { 1.f, 1.f }, lPivot).TransformPoint(lPivot), lPivot);

    // The translation moves the pivot too.
    CheckPoint(Affine2D::FromPose({ 5.f, -5.f }, 65.f, { 2.f, 2.f }, lPivot).TransformPoint(lPivot), { 45.f, 15.f });

    // A point right of the pivot, turned a quarter, goes above it.
    CheckPoint(Affine2D::FromPose({ 0.f, 0.f }, 90.f, { 1.f, 1.f }, lPivot).TransformPoint({ 50.f, 20.f }), { 40.f, 30.f });
}

TEST_CASE("Affine2D: a product is the right one first, then the left one")
{
    const Affine2D lMove  = Affine2D::FromTranslationRotationScale({ 10.f, 0.f }, 0.f, { 1.f, 1.f });
    const Affine2D lScale = Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 0.f, { 2.f, 2.f });

    // Scale then move: (1, 1) -> (2, 2) -> (12, 2).
    CheckPoint((lMove * lScale).TransformPoint({ 1.f, 1.f }), { 12.f, 2.f });
    // Move then scale: (1, 1) -> (11, 1) -> (22, 2).
    CheckPoint((lScale * lMove).TransformPoint({ 1.f, 1.f }), { 22.f, 2.f });

    const Affine2D lA = Affine2D::FromPose({ 3.f, 1.f }, 20.f, { 1.2f, 1.2f }, { 5.f, 5.f });
    const Affine2D lB = Affine2D::FromPose({ -2.f, 4.f }, -50.f, { 0.5f, 0.5f }, { 1.f, 2.f });
    const Vector2F lPoint{ 7.f, -3.f };
    CheckPoint((lA * lB).TransformPoint(lPoint), lA.TransformPoint(lB.TransformPoint(lPoint)));
}

TEST_CASE("Affine2D: decompose gives back what built it")
{
    const Affine2D lBuilt = Affine2D::FromTranslationRotationScale({ 12.f, -8.f }, 33.f, { 1.5f, 0.5f });
    const Affine2DParts lParts = lBuilt.Decompose();

    CheckPoint(lParts.Translation, { 12.f, -8.f });
    CHECK(lParts.Rotation == doctest::Approx(33.f));
    CheckPoint(lParts.Scale, { 1.5f, 0.5f });

    // A turn past 180 reads back as its negative twin.
    CHECK(Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 270.f, { 1.f, 1.f }).Decompose().Rotation == doctest::Approx(-90.f));

    // A flip keeps its sign on y.
    CHECK(Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 0.f, { 1.f, -1.f }).Decompose().Scale.y == doctest::Approx(-1.f));
}

TEST_CASE("Affine2D: a box turned 90 degrees swaps its width and height")
{
    const Bounds2D lBox = Bounds2D::FromCenterSize({ 10.f, 0.f }, { 100.f, 20.f });

    const Bounds2D lTurned = Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 90.f, { 1.f, 1.f }).TransformBox(lBox);
    CheckPoint(lTurned.Center, { 0.f, 10.f });
    CheckPoint(lTurned.Size(), { 20.f, 100.f });

    // At 45 degrees the cover grows: the same as Bounds2D's own rotated cover.
    const Bounds2D lCover = Affine2D::FromTranslationRotationScale({ 0.f, 0.f }, 45.f, { 1.f, 1.f }).TransformBox(
        Bounds2D::FromCenterSize({ 0.f, 0.f }, { 100.f, 20.f }));
    const Bounds2D lExpected = Bounds2D::FromCenterSizeRotated({ 0.f, 0.f }, { 100.f, 20.f }, Maths::DegreesToRadians(45.f));
    CheckPoint(lCover.Size(), lExpected.Size());
}
