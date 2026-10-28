// Suite: ResolveDisplayPose and EntityHierarchy::DisplayTransform — the fixed-step interpolation
// blend. Pure, so the arithmetic can be tested directly: the shortest-arc wrap, the first step (no
// previous pose), and a chain drawn with its blended parents.
#include <doctest.h>

#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/TransformInterpolation.h"
#include "World/World.h"

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
    const TransformInterpolationComponent lUnwritten{};
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

// =============================================================================
// The blended pose composes up the chain
// =============================================================================
TEST_CASE("ResolveDisplayPose: the display pose carries the current SCALE, unblended")
{
    TransformComponent lCurrent = At(0.f, 0.f);
    lCurrent.Scale = { 3.f, 0.5f };

    const DisplayPose lPose = ResolveDisplayPose(lCurrent, nullptr, 0.5f);
    CHECK(lPose.Scale.x == doctest::Approx(3.f));
    CHECK(lPose.Scale.y == doctest::Approx(0.5f));
}

TEST_CASE("DisplayTransform: a child of an interpolated parent is drawn where the parent is DRAWN, not where it is")
{
    World lWorld("Blend");

    Entity lParent = lWorld.CreateEntity("Body");
    Entity lChild  = lWorld.CreateEntity("Sprite");

    lParent.Get<TransformComponent>() = At(100.f, 0.f);     // the current fixed-step pose
    lChild.Get<TransformComponent>()  = At(10.f, 0.f);      // local: 10 to the right of the body
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent, /*bKeepWorld*/false));

    lParent.Add<TransformInterpolationComponent>(Prev(0.f, 0.f));   // the body was at 0 a step ago

    const TransformComponent lDrawn = EntityHierarchy::DisplayTransform(lChild, 0.5f);

    // Half a step in, the body draws at 50, and the sprite rides it to 60, not 110.
    CHECK(lDrawn.Position.x == doctest::Approx(60.f));
    CHECK(EntityHierarchy::WorldTransform(lChild).Position.x == doctest::Approx(110.f));   // the RAW world, for gameplay
}

TEST_CASE("DisplayTransform: a child turns with the parent's BLENDED rotation")
{
    World lWorld("Turn");

    Entity lParent = lWorld.CreateEntity("Body");
    Entity lChild  = lWorld.CreateEntity("Sprite");

    lParent.Get<TransformComponent>() = At(0.f, 0.f, 90.f);
    lChild.Get<TransformComponent>()  = At(10.f, 0.f);
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent, /*bKeepWorld*/false));

    lParent.Add<TransformInterpolationComponent>(Prev(0.f, 0.f, 0.f));

    // Half a step in, the body draws at 45 degrees: the child sits on that diagonal.
    const TransformComponent lDrawn = EntityHierarchy::DisplayTransform(lChild, 0.5f);
    CHECK(lDrawn.Rotation == doctest::Approx(45.f));
    CHECK(lDrawn.Position.x == doctest::Approx(7.0710678f));
    CHECK(lDrawn.Position.y == doctest::Approx(7.0710678f));
}

TEST_CASE("DisplayTransform: without previous poses it is the world transform, at any alpha")
{
    World lWorld("Still");

    Entity lRoot  = lWorld.CreateEntity("Root");
    Entity lMid   = lWorld.CreateEntity("Mid");
    Entity lLeaf  = lWorld.CreateEntity("Leaf");

    lRoot.Get<TransformComponent>() = At(5.f, -3.f, 30.f);
    lMid.Get<TransformComponent>()  = At(20.f, 0.f, 15.f);
    lLeaf.Get<TransformComponent>() = At(0.f, 8.f);
    REQUIRE(EntityHierarchy::SetParent(lMid, lRoot, /*bKeepWorld*/false));
    REQUIRE(EntityHierarchy::SetParent(lLeaf, lMid, /*bKeepWorld*/false));

    const TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lLeaf);

    for (const float lAlpha : { 0.f, 0.4f, 1.f })
    {
        const TransformComponent lDrawn = EntityHierarchy::DisplayTransform(lLeaf, lAlpha);
        CHECK(lDrawn.Position.x == lWorldXf.Position.x);
        CHECK(lDrawn.Position.y == lWorldXf.Position.y);
        CHECK(lDrawn.Rotation == lWorldXf.Rotation);
    }
}

// =============================================================================
// Poses the world records around each fixed step (TransformInterpolation)
// =============================================================================
TEST_CASE("ResolveDisplayPose: a pose the last step left blends; a pose moved since (in Update) is drawn as it is")
{
    TransformInterpolationComponent lHistory = Prev(0.f, 0.f);
    lHistory.SteppedPosition = { 100.f, 0.f };
    lHistory.bHasStepped     = true;

    // Where the step left it: half way between the two steps.
    CHECK(ResolveDisplayPose(At(100.f, 0.f), &lHistory, 0.5f).Position.x == doctest::Approx(50.f));

    // Moved to 130 after the step: drawn at 130, not pulled back toward 0.
    CHECK(ResolveDisplayPose(At(130.f, 0.f), &lHistory, 0.5f).Position.x == doctest::Approx(130.f));

    // Turned after the step: the same.
    CHECK(ResolveDisplayPose(At(100.f, 0.f, 20.f), &lHistory, 0.5f).RotationDeg == doctest::Approx(20.f));
}

TEST_CASE("TransformInterpolation: the world records the pose at the start and the end of a step, for the entities that ask")
{
    World lWorld("Recorded");

    Entity lCamera = lWorld.CreateEntity("Camera");
    lCamera.Get<TransformComponent>() = At(10.f, 0.f, 5.f);
    TransformInterpolation::RecordSteps(lCamera);
    TransformInterpolation::RecordSteps(lCamera);   // again: nothing changes

    REQUIRE(lCamera.TryGet<TransformInterpolationComponent>() != nullptr);
    CHECK_FALSE(lCamera.Get<TransformInterpolationComponent>().bHasPrevious);

    TransformInterpolation::BeginFixedStep(lWorld);
    lCamera.Get<TransformComponent>() = At(40.f, 0.f, 15.f);   // moved by the step
    TransformInterpolation::EndFixedStep(lWorld);

    const TransformInterpolationComponent& lHistory = lCamera.Get<TransformInterpolationComponent>();
    CHECK(lHistory.bHasPrevious);
    CHECK(lHistory.Position.x == doctest::Approx(10.f));
    CHECK(lHistory.Rotation == doctest::Approx(5.f));
    CHECK(lHistory.bHasStepped);
    CHECK(lHistory.SteppedPosition.x == doctest::Approx(40.f));
    CHECK(lHistory.SteppedRotation == doctest::Approx(15.f));

    // Drawn between the two steps.
    CHECK(EntityHierarchy::DisplayTransform(lCamera, 0.5f).Position.x == doctest::Approx(25.f));
}

TEST_CASE("TransformInterpolation: what physics or the mover records is left alone")
{
    World lWorld("Bodies");

    // A body: its previous pose is written by the physics step, not by the world.
    Entity lBody = lWorld.CreateEntity("Body");
    lBody.Get<TransformComponent>() = At(100.f, 0.f);
    lBody.Add<TransformInterpolationComponent>(Prev(70.f, 0.f));

    TransformInterpolation::BeginFixedStep(lWorld);
    TransformInterpolation::EndFixedStep(lWorld);

    const TransformInterpolationComponent& lHistory = lBody.Get<TransformInterpolationComponent>();
    CHECK(lHistory.Position.x == doctest::Approx(70.f));
    CHECK_FALSE(lHistory.bHasStepped);

    // A body that also asks keeps its previous pose until the next step records one.
    TransformInterpolation::RecordSteps(lBody);
    CHECK(lBody.Get<TransformInterpolationComponent>().Position.x == doctest::Approx(70.f));
}
