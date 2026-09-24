// Suite: the physics SEAM (Physics/PhysicsAPI.h + Physics/IPhysicsWorld.h).
//
// This suite runs the real Box2D backend through the neutral interface and NOTHING ELSE — it never
// names a b2* type, and it cannot: box2d is PRIVATE to OpaaxEngine.dll and its include dir is
// private too. That is the point. OpaaxTests links the engine's import lib exactly like a game exe,
// so a case that creates a world here is simultaneously a proof that the seam is properly exported
// and that no consumer needs the vendor (L11).
//
// Physics is one of the very few engine paths that needs no GL context, so unlike the renderer it
// can be gated by a test rather than by a smoke run and a pair of eyes.
#include <doctest.h>

#include "Physics/Collision/CollisionChannel.h"
#include "Physics/PhysicsAPI.h"

using namespace Opaax;

namespace
{
    /** The engine's own defaults: Y-up, ~100 units = 1 m, gravity -981. */
    PhysicsWorldDesc MakeDesc()
    {
        PhysicsWorldDesc lDesc;
        lDesc.Gravity             = { 0.f, -981.f };
        lDesc.LengthUnitsPerMeter = 100.f;
        lDesc.SubStepCount        = 4;
        return lDesc;
    }

    /** Advance InSeconds at the engine's 60 Hz fixed step. */
    void StepFor(IPhysicsWorld& InWorld, float InSeconds, int InSubSteps = 4)
    {
        constexpr float lFixedStep = 1.f / 60.f;

        const int lSteps = static_cast<int>(InSeconds / lFixedStep);
        for (int i = 0; i < lSteps; ++i)
        {
            InWorld.Step(lFixedStep, InSubSteps);
        }
    }

    /** A static floor whose TOP surface sits at Y = 0. */
    BodyHandle AddFloor(IPhysicsWorld& InWorld, Uint64 InUserData = 0)
    {
        BodyDesc lBody;
        lBody.Type     = EBodyType::Static;
        lBody.Position = { 0.f, -50.f };
        lBody.UserData = InUserData;

        const BodyHandle lHandle = InWorld.CreateBody(lBody);

        ShapeDesc lShape;
        lShape.Geometry.Type        = EColliderShape::Box;
        lShape.Geometry.HalfExtents = { 500.f, 50.f };
        InWorld.AddShape(lHandle, lShape);

        return lHandle;
    }
}

// =============================================================================
// The factory
// =============================================================================

TEST_CASE("PhysicsAPI: Create returns a live world for the only backend there is")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());

    REQUIRE(lWorld != nullptr);
    CHECK(PhysicsAPI::GetBackend() == EPhysicsBackend::Box2D);
    CHECK(ToString(EPhysicsBackend::Box2D) == doctest::String("Box2D"));
}

TEST_CASE("PhysicsAPI: the world round-trips gravity through the seam")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    CHECK(lWorld->GetGravity().y == doctest::Approx(-981.f));

    lWorld->SetGravity({ 0.f, -100.f });
    CHECK(lWorld->GetGravity().y == doctest::Approx(-100.f));
}

// =============================================================================
// Bodies — the P0 gate: a body falls, and it lands
// =============================================================================

TEST_CASE("IPhysicsWorld: a dynamic body falls under gravity")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    BodyDesc lBody;
    lBody.Type     = EBodyType::Dynamic;
    lBody.Position = { 0.f, 1000.f };

    const BodyHandle lHandle = lWorld->CreateBody(lBody);
    REQUIRE(lHandle.IsValid());

    ShapeDesc lShape;
    lShape.Geometry.Type        = EColliderShape::Box;
    lShape.Geometry.HalfExtents = { 25.f, 25.f };
    REQUIRE(lWorld->AddShape(lHandle, lShape).IsValid());

    StepFor(*lWorld, 0.5f);

    Vector2F lPos;
    float    lRot = 0.f;
    lWorld->GetBodyTransform(lHandle, lPos, lRot);

    // Free fall for half a second at 981 u/s^2 is ~123 units. Asserting only "it moved DOWN" would
    // pass for a body that drifted one unit, so this pins the order of magnitude.
    CHECK(lPos.y < 950.f);
    CHECK(lPos.y > 700.f);
}

TEST_CASE("IPhysicsWorld: a falling body comes to REST on static geometry")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    AddFloor(*lWorld);

    BodyDesc lBody;
    lBody.Type     = EBodyType::Dynamic;
    lBody.Position = { 0.f, 500.f };

    const BodyHandle lFaller = lWorld->CreateBody(lBody);
    REQUIRE(lFaller.IsValid());

    ShapeDesc lShape;
    lShape.Geometry.Type        = EColliderShape::Box;
    lShape.Geometry.HalfExtents = { 25.f, 25.f };
    lWorld->AddShape(lFaller, lShape);

    StepFor(*lWorld, 3.f);

    Vector2F lPos;
    float    lRot = 0.f;
    lWorld->GetBodyTransform(lFaller, lPos, lRot);

    // Floor top is Y=0 and the box's half-height is 25, so it rests at ~25 — NOT merely "below
    // where it started", which a body that fell through the floor forever would also satisfy.
    CHECK(lPos.y == doctest::Approx(25.f).epsilon(0.05));

    // And it must have STOPPED: another second of stepping may not move it.
    StepFor(*lWorld, 1.f);

    Vector2F lSettled;
    lWorld->GetBodyTransform(lFaller, lSettled, lRot);
    CHECK(lSettled.y == doctest::Approx(lPos.y).epsilon(0.01));
}

TEST_CASE("IPhysicsWorld: a static body does not move, and DestroyBody is safe on an invalid handle")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    const BodyHandle lFloor = AddFloor(*lWorld);
    StepFor(*lWorld, 1.f);

    Vector2F lPos;
    float    lRot = 0.f;
    lWorld->GetBodyTransform(lFloor, lPos, lRot);
    CHECK(lPos.y == doctest::Approx(-50.f));

    lWorld->DestroyBody(BodyHandle{});   // no-op, must not trap
    lWorld->DestroyBody(lFloor);
}

// =============================================================================
// Queries — resolved to the caller's own user-data, never to a backend id
// =============================================================================

TEST_CASE("IPhysicsWorld: a ray finds the floor and carries its user-data back")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    constexpr Uint64 lFloorTag = 4242u;
    AddFloor(*lWorld, lFloorTag);
    StepFor(*lWorld, 0.1f);

    const PhysicsRayHit lHit = lWorld->RayCastClosest({ 0.f, 200.f }, { 0.f, -1.f }, 500.f,
                                                      AllChannelsMask());

    REQUIRE(lHit.bHit);
    CHECK(lHit.UserData == lFloorTag);
    CHECK(lHit.Point.y == doctest::Approx(0.f).epsilon(0.05));   // the floor's top surface
    CHECK(lHit.Normal.y > 0.9f);                                 // pointing up, at the ray
}

TEST_CASE("IPhysicsWorld: a ray pointing away from everything misses")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    AddFloor(*lWorld);
    StepFor(*lWorld, 0.1f);

    const PhysicsRayHit lHit = lWorld->RayCastClosest({ 0.f, 200.f }, { 0.f, 1.f }, 500.f,
                                                      AllChannelsMask());
    CHECK_FALSE(lHit.bHit);
    CHECK(lHit.UserData == 0u);
}

TEST_CASE("IPhysicsWorld: OverlapAABB collects the user-data of what it covers, and clears first")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    constexpr Uint64 lFloorTag = 77u;
    AddFloor(*lWorld, lFloorTag);
    StepFor(*lWorld, 0.1f);

    TDynArray<Uint64> lHits;
    lHits.push_back(999u);   // stale content from a previous query must not survive

    lWorld->OverlapAABB({ -100.f, -100.f }, { 100.f, 10.f }, AllChannelsMask(), lHits);

    REQUIRE(lHits.size() == 1u);
    CHECK(lHits[0] == lFloorTag);

    // Far away from the floor: the array is emptied even when nothing is found.
    lWorld->OverlapAABB({ 5000.f, 5000.f }, { 5100.f, 5100.f }, AllChannelsMask(), lHits);
    CHECK(lHits.empty());
}

// =============================================================================
// Channel filtering — the mask is what a collider's channel means at runtime
// =============================================================================

TEST_CASE("CollisionChannel: a channel's bit is its ordinal, and names round-trip")
{
    CHECK(CategoryBit(ECollisionChannel::WorldStatic) == 1ull);
    CHECK(CategoryBit(ECollisionChannel::WorldDynamic) == 2ull);
    CHECK(CategoryBit(ECollisionChannel::Pawn) == 4ull);

    // Asserted against the ENUM rather than a hardcoded ordinal, so appending a channel cannot
    // break a test that is about bit positions (the L10 lesson, one directory over).
    for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
    {
        const auto lChannel = static_cast<ECollisionChannel>(i);
        CHECK(CollisionChannelFromStringID(ToStringID(lChannel)) == lChannel);
        CHECK(CategoryBit(lChannel) == (Uint64(1) << i));
    }
}

TEST_CASE("IPhysicsWorld: a ray filtered to a channel the shape is NOT on misses it")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    BodyDesc lBody;
    lBody.Type     = EBodyType::Static;
    lBody.Position = { 0.f, 0.f };
    lBody.UserData = 5u;

    const BodyHandle lHandle = lWorld->CreateBody(lBody);

    ShapeDesc lShape;
    lShape.Geometry.Type        = EColliderShape::Box;
    lShape.Geometry.HalfExtents = { 100.f, 100.f };
    lShape.CategoryBits         = CategoryBit(ECollisionChannel::Pawn);
    lWorld->AddShape(lHandle, lShape);

    StepFor(*lWorld, 0.1f);

    const PhysicsRayHit lOnPawn = lWorld->RayCastClosest({ 0.f, 500.f }, { 0.f, -1.f }, 1000.f,
                                                         CategoryBit(ECollisionChannel::Pawn));
    CHECK(lOnPawn.bHit);

    const PhysicsRayHit lOnProjectile = lWorld->RayCastClosest({ 0.f, 500.f }, { 0.f, -1.f }, 1000.f,
                                                               CategoryBit(ECollisionChannel::Projectile));
    CHECK_FALSE(lOnProjectile.bHit);
}

// =============================================================================
// The geometric mover — the character-controller primitive P5 is built on
// =============================================================================

TEST_CASE("IPhysicsWorld: MoveCapsule reports grounded when it lands on the floor")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    AddFloor(*lWorld);
    StepFor(*lWorld, 0.1f);

    MoveCapsuleInput lInput;
    lInput.Position        = { 0.f, 60.f };
    lInput.Capsule.Center1 = { 0.f, -20.f };
    lInput.Capsule.Center2 = { 0.f, 20.f };
    lInput.Capsule.Radius  = 25.f;
    lInput.Velocity        = { 0.f, -600.f };
    lInput.DeltaTime       = 1.f / 60.f;
    lInput.ChannelMask     = AllChannelsMask();

    // Drive it down until it settles on the floor.
    for (int i = 0; i < 30; ++i)
    {
        const MoveCapsuleResult lResult = lWorld->MoveCapsule(lInput);
        lInput.Position = lResult.Position;

        if (lResult.bGrounded)
        {
            CHECK(lResult.GroundNormal.y > 0.9f);
            CHECK(lResult.Velocity.y == doctest::Approx(0.f).epsilon(0.01));   // clipped by the floor
            return;
        }
    }

    FAIL("MoveCapsule never reported grounded after falling onto the floor");
}

TEST_CASE("IPhysicsWorld: MoveCapsule in open space just moves, and is not grounded")
{
    const TUniquePtr<IPhysicsWorld> lWorld = PhysicsAPI::Create(EPhysicsBackend::Box2D, MakeDesc());
    REQUIRE(lWorld != nullptr);

    StepFor(*lWorld, 0.1f);

    MoveCapsuleInput lInput;
    lInput.Position        = { 0.f, 1000.f };
    lInput.Capsule.Center1 = { 0.f, -20.f };
    lInput.Capsule.Center2 = { 0.f, 20.f };
    lInput.Capsule.Radius  = 25.f;
    lInput.Velocity        = { 300.f, 0.f };
    lInput.DeltaTime       = 1.f / 60.f;
    lInput.ChannelMask     = AllChannelsMask();

    const MoveCapsuleResult lResult = lWorld->MoveCapsule(lInput);

    CHECK_FALSE(lResult.bGrounded);
    CHECK(lResult.Position.x > 1.f);                       // it advanced along the velocity
    CHECK(lResult.Position.y == doctest::Approx(1000.f));  // and only along it
}
