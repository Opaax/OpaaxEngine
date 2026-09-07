// Suite: ResolveDisplayPose — the fixed-step interpolation blend (⑦-A P6).
//
// Free and pure for ToQuad's reason: the arithmetic is what can be wrong, and it needs no world,
// no GL context and no clock. The cases that matter are the ones a screenshot cannot judge — the
// shortest-arc wrap, and the FIRST step, which has no previous pose to blend from.
#include <doctest.h>

#include "World/Components/TransformInterpolationComponent.h"

using namespace Opaax;

namespace
{
    TransformComponent At(const float InX, const float InY, const float InRot = 0.f)
    {
        TransformComponent lXf;
        lXf.Position = { InX, InY };
        lXf.Rotation = InRot;
        return lXf;
    }

    TransformInterpolationComponent Prev(const float InX, const float InY, const float InRot = 0.f)
    {
        TransformInterpolationComponent lPrev;
        lPrev.Position     = { InX, InY };
        lPrev.Rotation     = InRot;
        lPrev.bHasPrevious = true;
        return lPrev;
    }
}

TEST_CASE("ResolveDisplayPose: no previous pose answers the CURRENT one exactly")
{
    const TransformComponent lCurrent = At(100.f, 200.f, 45.f);

    // The common case by far — anything not driven by a fixed step has no interpolation component
    // at all, and must draw exactly where it is.
    const DisplayPose lNone = ResolveDisplayPose(lCurrent, nullptr, 0.5f);
    CHECK(lNone.Position.x == doctest::Approx(100.f));
    CHECK(lNone.RotationDeg == doctest::Approx(45.f));

    // And the FIRST step: the component exists but has never been written. Blending from a
    // default-constructed pose would fling the entity in from the origin.
    const TransformInterpolationComponent lUnwritten;
    const DisplayPose lFirst = ResolveDisplayPose(lCurrent, &lUnwritten, 0.5f);
    CHECK(lFirst.Position.x == doctest::Approx(100.f));
    CHECK(lFirst.Position.y == doctest::Approx(200.f));
}

TEST_CASE("ResolveDisplayPose: alpha 0 is the PREVIOUS pose and 1 is the current one")
{
    const TransformComponent              lCurrent = At(100.f, 0.f);
    const TransformInterpolationComponent lPrev    = Prev(0.f, 0.f);

    CHECK(ResolveDisplayPose(lCurrent, &lPrev, 0.f).Position.x == doctest::Approx(0.f));
    CHECK(ResolveDisplayPose(lCurrent, &lPrev, 1.f).Position.x == doctest::Approx(100.f));
    CHECK(ResolveDisplayPose(lCurrent, &lPrev, 0.25f).Position.x == doctest::Approx(25.f));
}

TEST_CASE("ResolveDisplayPose: alpha is CLAMPED, never extrapolated")
{
    const TransformComponent              lCurrent = At(100.f, 0.f);
    const TransformInterpolationComponent lPrev    = Prev(0.f, 0.f);

    // Overshooting would draw a pose the simulation has not produced — a different feature, and a
    // worse default than simply pinning to the ends.
    CHECK(ResolveDisplayPose(lCurrent, &lPrev, 2.f).Position.x == doctest::Approx(100.f));
    CHECK(ResolveDisplayPose(lCurrent, &lPrev, -1.f).Position.x == doctest::Approx(0.f));
}

TEST_CASE("ResolveDisplayPose: rotation takes the SHORTEST ARC across the wrap")
{
    // 350 -> 10 is +20 degrees the short way. A naive lerp travels -340, which is a visible spin
    // the body never performed — the whole reason the delta is wrapped before blending.
    const TransformComponent              lCurrent = At(0.f, 0.f, 10.f);
    const TransformInterpolationComponent lPrev    = Prev(0.f, 0.f, 350.f);

    const DisplayPose lHalf = ResolveDisplayPose(lCurrent, &lPrev, 0.5f);

    // Half of +20 from 350 is 360 — the same angle as 0, and NOT the 180 a backwards lerp gives.
    CHECK(lHalf.RotationDeg == doctest::Approx(360.f));

    // The other direction wraps too: 10 -> 350 is -20 the short way.
    const TransformComponent              lBack     = At(0.f, 0.f, 350.f);
    const TransformInterpolationComponent lPrevBack = Prev(0.f, 0.f, 10.f);
    CHECK(ResolveDisplayPose(lBack, &lPrevBack, 0.5f).RotationDeg == doctest::Approx(0.f));
}

TEST_CASE("ResolveDisplayPose: an ordinary rotation is a plain blend")
{
    const TransformComponent              lCurrent = At(0.f, 0.f, 90.f);
    const TransformInterpolationComponent lPrev    = Prev(0.f, 0.f, 0.f);

    // No wrap involved, so the shortest arc IS the direct one — the wrap must not perturb it.
    CHECK(ResolveDisplayPose(lCurrent, &lPrev, 0.5f).RotationDeg == doctest::Approx(45.f));
}

TEST_CASE("ResolveDisplayPose: a mover that did not move draws where it is")
{
    const TransformComponent              lCurrent = At(42.f, -7.f, 33.f);
    const TransformInterpolationComponent lPrev    = Prev(42.f, -7.f, 33.f);

    // The resting case, which is most entities most of the time: blending equal poses must be
    // exactly the pose, at every alpha, or everything would shimmer.
    for (const float lAlpha : { 0.f, 0.33f, 0.5f, 1.f })
    {
        const DisplayPose lPose = ResolveDisplayPose(lCurrent, &lPrev, lAlpha);
        CHECK(lPose.Position.x == doctest::Approx(42.f));
        CHECK(lPose.Position.y == doctest::Approx(-7.f));
        CHECK(lPose.RotationDeg == doctest::Approx(33.f));
    }
}
