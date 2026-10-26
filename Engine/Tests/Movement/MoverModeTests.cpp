// Suite: the mover runtime — the two engine modes as classes, driven directly against a real
// physics world (MoverSubsystem itself needs a started engine).
#include <doctest.h>

#include "Core/Algo/Algo.h"

#include "Movement/Assets/MoveMode.h"
#include "Physics/PhysicsAPI.h"
#include "Movement/MoverComponent.h"
#include "World/Components/TransformComponent.h"
#include "Movement/Modes/FlyMoveMode.h"
#include "Movement/Modes/GroundMoveMode.h"

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
    void Drive(const MoveMode& InMode, IPhysicsWorld& InWorld, MoverComponent& InMover,
               TransformComponent& InTransform, const int InSteps)
    {
        for (int i = 0; i < InSteps; ++i)
        {
            MoverTickContext lTick{ InWorld, InMover, InTransform, 1.f / 60.f, 0 };
            InMode.Tick(lTick);
        }
    }

    /** InClass's property names, parents' first. */
    TDynArray<OpaaxString> PropertyNames(const ObjectClass& InClass)
    {
        TDynArray<OpaaxString> lNames;
        for (const char* lName : InClass.GetPropertyNames()) { lNames.emplace_back(lName); }
        return lNames;
    }
}

// =============================================================================
// The modes are classes
// =============================================================================

TEST_CASE("MoveMode: each mode is a subclass, and lists only the tuning it reads")
{
    // The base holds what every mode shares and cannot be made by itself: a file names a mode.
    CHECK(MoveMode::StaticClass().IsAbstract());
    CHECK(GroundMoveMode::StaticClass().IsChildOf(MoveMode::StaticClass()));
    CHECK(FlyMoveMode::StaticClass().IsChildOf(MoveMode::StaticClass()));

    const TDynArray<OpaaxString> lFly = PropertyNames(FlyMoveMode::StaticClass());
    REQUIRE(lFly.size() == 2u);
    CHECK(lFly[0] == "MaxSlopeAngleDeg");   // the base's first
    CHECK(lFly[1] == "MaxSpeed");

    const TDynArray<OpaaxString> lGround = PropertyNames(GroundMoveMode::StaticClass());
    CHECK(lGround.size() == 9u);
    CHECK(Algo::Find(lGround, OpaaxString("JumpSpeed")) != lGround.end());
    CHECK(Algo::Find(lFly, OpaaxString("JumpSpeed")) == lFly.end());
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


    Drive(lMode, *lWorld, lMover, lTransform, 180);

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


    Drive(lMode, *lWorld, lMover, lTransform, 30);   // settle
    const float lRestX = lTransform.Position.x;

    lMover.Input.MoveDir = { 1.f, 0.f };
    Drive(lMode, *lWorld, lMover, lTransform, 60);
    CHECK(lTransform.Position.x > lRestX + 50.f);

    const float lRightX = lTransform.Position.x;

    lMover.Input.MoveDir = { -1.f, 0.f };
    Drive(lMode, *lWorld, lMover, lTransform, 60);
    CHECK(lTransform.Position.x < lRightX);
}

TEST_CASE("GroundMoveMode: a jump LEAVES the ground, and the edge is consumed")
{
    const TUniquePtr<IPhysicsWorld> lWorld = MakeWorldWithFloor();

    GroundMoveMode     lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;
    lTransform.Position = { 0.f, 60.f };


    Drive(lMode, *lWorld, lMover, lTransform, 30);
    REQUIRE(lMover.bGrounded);

    const float lGroundY = lTransform.Position.y;

    lMover.Input.bJump = true;
    Drive(lMode, *lWorld, lMover, lTransform, 1);

    // The edge is spent by the mode, so ONE request is one jump however many steps a frame runs.
    CHECK_FALSE(lMover.Input.bJump);

    Drive(lMode, *lWorld, lMover, lTransform, 10);
    CHECK(lTransform.Position.y > lGroundY + 20.f);
    CHECK_FALSE(lMover.bGrounded);

    // ...and it comes back down without needing another request.
    Drive(lMode, *lWorld, lMover, lTransform, 180);
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


    Drive(lMode, *lWorld, lMover, lTransform, 5);
    REQUIRE_FALSE(lMover.bGrounded);

    const float lBefore = lTransform.Position.y;

    lMover.Input.bJump = true;
    Drive(lMode, *lWorld, lMover, lTransform, 5);

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

    lMode.GravityScale = 0.f;

    Drive(lMode, *lWorld, lMover, lTransform, 60);

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

    lMode.MaxSpeed = 300.f;

    // Up and right — GroundMove reads x alone, so the y is what distinguishes the two modes.
    lMover.Input.MoveDir = { 1.f, 1.f };
    Drive(lMode, *lWorld, lMover, lTransform, 30);

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

    Drive(lMode, *lWorld, lMover, lTransform, 60);   // no intent at all

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

    Drive(lGround, *lWorld, lMover, lTransform, 20);
    REQUIRE(lMover.Velocity.y < -50.f);   // genuinely falling

    MoverTickContext lEnter{ *lWorld, lMover, lTransform, 0.f, 0 };
    lFly.OnModeEnter(lEnter);

    // Without this the thing keeps sinking for a moment after switching, which reads as a bug.
    CHECK(lMover.Velocity.y == doctest::Approx(0.f));
}

// =============================================================================
// Owner — a mode reads its entity's other components (a game mode's own state)
// =============================================================================

namespace
{
    struct BoostComponent
    {
        float Speed = 0.f;
    };

    /** Takes its horizontal speed from a sibling component, the way a game mode reads its tuning. */
    class BoostMoveMode final : public MoveMode
    {
        OPAAX_OBJECT(BoostMoveMode, MoveMode)

    public:
        void Tick(MoverTickContext& InContext) const override
        {
            const BoostComponent* lBoost = InContext.Owner ? InContext.Owner.TryGet<BoostComponent>() : nullptr;
            InContext.Mover.Velocity.x = lBoost != nullptr ? lBoost->Speed : -1.f;
        }
    };
}

TEST_CASE("MoveMode: Owner reaches the entity's other components")
{
    const TUniquePtr<IPhysicsWorld> lPhysics = MakeWorldWithFloor();

    World  lWorld("Owner");
    Entity lEntity = lWorld.CreateEntity("Boosted");
    lEntity.Add<BoostComponent>(BoostComponent{ 321.f });

    BoostMoveMode      lMode;
    MoverComponent     lMover = MakeMover();
    TransformComponent lTransform;

    MoverTickContext lTick{ *lPhysics, lMover, lTransform, 1.f / 60.f, 0, lEntity };
    lMode.Tick(lTick);
    CHECK(lMover.Velocity.x == doctest::Approx(321.f));

    // A context built without an owner is invalid rather than pointing anywhere.
    MoverTickContext lNoOwner{ *lPhysics, lMover, lTransform, 1.f / 60.f, 0 };
    CHECK_FALSE(lNoOwner.Owner.IsValid());
    lMode.Tick(lNoOwner);
    CHECK(lMover.Velocity.x == doctest::Approx(-1.f));
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
