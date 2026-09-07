// Suite: the mover RUNTIME — the mode registry, and the two modes' policy (⑦-A P5b).
//
// MoverSubsystem itself needs a started engine (its Startup resolves EngineRegistries through the
// locator), so what is testable headlessly is the registry and the MODES, driven directly against
// a real physics world. That is the half worth pinning anyway: a mode is pure policy plus one
// MoveCapsule call, and the policy is where "almost right" hides — a jump that does not clear the
// ground, gravity that fights the sweep, an air-steer that silently equals the ground one.
//
// The subsystem's own wiring (resolving a bag, switching modes, the ref caches) is gated by the
// hosts' ordered boot log, exactly as WorldSubsystemRegistryTests says for its own case.
#include <doctest.h>

#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Physics/PhysicsAPI.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Systems/Movement/FlyMoveMode.h"
#include "World/Systems/Movement/GroundMoveMode.h"
#include "World/Systems/Movement/MoverModeRegistry.h"

using namespace Opaax;

namespace
{
    /** A world with a floor whose TOP surface is at y = 0. */
    TUniquePtr<IPhysicsWorld> MakeWorldWithFloor()
    {
        PhysicsWorldDesc lDesc;
        lDesc.Gravity             = { 0.f, -981.f };
        lDesc.LengthUnitsPerMeter = 100.f;
        lDesc.SubStepCount        = 4;

        TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, lDesc);
        REQUIRE(lWorld != nullptr);

        BodyDesc lBody;
        lBody.Type     = EBodyType::Static;
        lBody.Position = { 0.f, -50.f };
        lBody.UserData = 1u;

        const BodyHandle lHandle = lWorld->CreateBody(lBody);

        ShapeDesc lShape;
        lShape.Geometry.Type        = EColliderShape::Box;
        lShape.Geometry.HalfExtents = { 2000.f, 50.f };
        lWorld->AddShape(lHandle, lShape);

        lWorld->Step(1.f / 60.f, 4);
        return lWorld;
    }

    MoverComponent MakeMover()
    {
        MoverComponent lMover;
        lMover.Height = 100.f;
        lMover.Radius = 25.f;
        return lMover;
    }

    /** Run InMode for InSteps fixed steps at 60 Hz. */
    void Drive(IMoverMode& InMode, IPhysicsWorld& InWorld, MoverComponent& InMover,
               TransformComponent& InTransform, const MoveModeData& InParams, const int InSteps)
    {
        for (int i = 0; i < InSteps; ++i)
        {
            MoverTickContext lTick{ InWorld, InMover, InTransform, InParams, 1.f / 60.f, 0 };
            InMode.Tick(lTick);
        }
    }
}

// =============================================================================
// The registry
// =============================================================================

TEST_CASE("MoverModeRegistry: registers by name and finds them")
{
    MoverModeRegistry lRegistry;

    REQUIRE(lRegistry.Register<GroundMoveMode>(OPAAX_ID("GroundMove")));
    REQUIRE(lRegistry.Register<FlyMoveMode>(OPAAX_ID("FlyMove")));

    CHECK(lRegistry.Count() == 2u);
    CHECK(lRegistry.Find(OPAAX_ID("GroundMove")) != nullptr);
    CHECK(lRegistry.Find(OPAAX_ID("FlyMove")) != nullptr);

    // A name nothing registered is a REAL state — a tuning may name a mode this build lacks.
    CHECK(lRegistry.Find(OPAAX_ID("SwimMove")) == nullptr);
    CHECK(lRegistry.Find(OpaaxStringID{}) == nullptr);
}

TEST_CASE("MoverModeRegistry: a duplicate is REFUSED, not replaced")
{
    MoverModeRegistry lRegistry;

    REQUIRE(lRegistry.Register<GroundMoveMode>(OPAAX_ID("Taken")));

    // Replacing would let a game module silently shadow a built-in that assets already name.
    CHECK_FALSE(lRegistry.Register<FlyMoveMode>(OPAAX_ID("Taken")));
    CHECK_FALSE(lRegistry.Register<FlyMoveMode>(OpaaxStringID{}));
    CHECK(lRegistry.Count() == 1u);
}

TEST_CASE("MoverModeRegistry: sealing refuses later registration")
{
    MoverModeRegistry lRegistry;

    REQUIRE(lRegistry.Register<GroundMoveMode>(OPAAX_ID("GroundMove")));
    lRegistry.Seal();

    CHECK(lRegistry.IsSealed());
    CHECK_FALSE(lRegistry.Register<FlyMoveMode>(OPAAX_ID("FlyMove")));
    CHECK(lRegistry.Count() == 1u);

    lRegistry.Seal();   // idempotent (LC3)
    CHECK(lRegistry.IsSealed());
}

// =============================================================================
// GroundMoveMode — the platformer policy
// =============================================================================

TEST_CASE("GroundMoveMode: a mover falls and LANDS on the floor")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 400.f };

    const MoveModeData lParams;   // the defaults are a walkable ground mode

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 180);

    CHECK(lMover.bGrounded);
    CHECK(lMover.GroundNormal.y > 0.9f);

    // The capsule's bottom rests ON the floor: half-height 50, so the centre sits at ~50.
    CHECK(lTransform.Position.y == doctest::Approx(50.f).epsilon(0.1));
}

TEST_CASE("GroundMoveMode: intent WALKS it, and the direction follows the sign")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 60.f };

    const MoveModeData lParams;

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 30);   // settle
    const float lRestX = lTransform.Position.x;

    lMover.Input.MoveDir = { 1.f, 0.f };
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 60);
    CHECK(lTransform.Position.x > lRestX + 50.f);

    const float lRightX = lTransform.Position.x;

    lMover.Input.MoveDir = { -1.f, 0.f };
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 60);
    CHECK(lTransform.Position.x < lRightX);
}

TEST_CASE("GroundMoveMode: a jump LEAVES the ground, and the edge is consumed")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 60.f };

    const MoveModeData lParams;

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 30);
    REQUIRE(lMover.bGrounded);

    const float lGroundY = lTransform.Position.y;

    lMover.Input.bJump = true;
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 1);

    // The edge is spent by the mode, so ONE request is one jump however many steps a frame runs.
    CHECK_FALSE(lMover.Input.bJump);

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 10);
    CHECK(lTransform.Position.y > lGroundY + 20.f);
    CHECK_FALSE(lMover.bGrounded);

    // ...and it comes back down without needing another request.
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 180);
    CHECK(lMover.bGrounded);
    CHECK(lTransform.Position.y == doctest::Approx(lGroundY).epsilon(0.1));
}

TEST_CASE("GroundMoveMode: a jump in MID-AIR is refused")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 600.f };   // falling, nothing underneath

    const MoveModeData lParams;

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 5);
    REQUIRE_FALSE(lMover.bGrounded);

    const float lBefore = lTransform.Position.y;

    lMover.Input.bJump = true;
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 5);

    // Still descending: an ungrounded jump must not spend, or the thing flies by holding Space.
    CHECK(lTransform.Position.y < lBefore);
}

TEST_CASE("GroundMoveMode: GravityScale 0 leaves it hanging")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 600.f };

    MoveModeData lParams;
    lParams.GravityScale = 0.f;

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 60);

    // The tuning reaches the policy: this is the field doing something a default cannot fake.
    CHECK(lTransform.Position.y == doctest::Approx(600.f).epsilon(0.01));
}

// =============================================================================
// FlyMoveMode — and the transition hook that makes a second mode worth having
// =============================================================================

TEST_CASE("FlyMoveMode: intent maps straight to velocity, on BOTH axes")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    FlyMoveMode        lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 400.f };

    MoveModeData lParams;
    lParams.Mode     = OPAAX_ID("FlyMove");
    lParams.MaxSpeed = 300.f;

    // Up and right — GroundMove reads x alone, so the y is what distinguishes the two modes.
    lMover.Input.MoveDir = { 1.f, 1.f };
    Drive(lMode, *lWorld, lMover, lTransform, lParams, 30);

    CHECK(lTransform.Position.x > 10.f);
    CHECK(lTransform.Position.y > 400.f);
}

TEST_CASE("FlyMoveMode: NO gravity — releasing the stick leaves it hanging")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    FlyMoveMode        lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 400.f };

    MoveModeData lParams;
    lParams.Mode = OPAAX_ID("FlyMove");

    Drive(lMode, *lWorld, lMover, lTransform, lParams, 60);   // no intent at all

    CHECK(lTransform.Position.y == doctest::Approx(400.f).epsilon(0.01));
    CHECK_FALSE(lMover.bGrounded);
}

TEST_CASE("FlyMoveMode: OnModeEnter drops momentum carried in from a fall")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lGround;
    FlyMoveMode        lFly;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 600.f };

    const MoveModeData lGroundParams;

    Drive(lGround, *lWorld, lMover, lTransform, lGroundParams, 20);
    REQUIRE(lMover.Velocity.y < -50.f);   // genuinely falling

    MoveModeData lFlyParams;
    lFlyParams.Mode = OPAAX_ID("FlyMove");

    MoverTickContext lEnter{ *lWorld, lMover, lTransform, lFlyParams, 0.f, 0 };
    lFly.OnModeEnter(lEnter);

    // Without this the thing keeps sinking for a moment after switching, which reads as a bug.
    CHECK(lMover.Velocity.y == doctest::Approx(0.f));
}

// =============================================================================
// The capsule the modes sweep
// =============================================================================

TEST_CASE("MoverComponent: BuildCapsule puts the caps a RADIUS in from each end")
{
    MoverComponent lMover = MakeMover();   // 100 tall, radius 25

    const MoverCapsule lCapsule = lMover.BuildCapsule();

    // Half-height 50 minus radius 25 = 25 each way, so the capsule is exactly 100 tall overall.
    CHECK(lCapsule.Center1.y == doctest::Approx(-25.f));
    CHECK(lCapsule.Center2.y == doctest::Approx(25.f));
    CHECK(lCapsule.Radius == doctest::Approx(25.f));

    // Too short for its radius degenerates to a CIRCLE rather than an inside-out capsule.
    lMover.Height = 30.f;
    const MoverCapsule lSquat = lMover.BuildCapsule();
    CHECK(lSquat.Center1.y == doctest::Approx(0.f));
    CHECK(lSquat.Center2.y == doctest::Approx(0.f));
}
