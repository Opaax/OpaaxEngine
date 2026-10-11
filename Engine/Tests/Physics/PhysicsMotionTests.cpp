// Suite: driving bodies from gameplay — velocities, impulses, forces and torque on dynamic bodies
// (built on demand for an entity created this frame), a kinematic body following its Transform,
// and a Transform moved by gameplay teleporting a dynamic body. Gravity is off unless a case
// says otherwise, so every expected value is plain arithmetic.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Events/EventBus.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Physics/Components/ColliderComponent.h"
#include "Physics/Components/RigidbodyComponent.h"
#include "Physics/PhysicsEvents.h"
#include "Physics/PhysicsSubsystem.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <functional>

using namespace Opaax;

namespace
{
    constexpr double STEP = 1.0 / 60.0;

    struct MotionFixture
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*            TheWorld = nullptr;
        PhysicsSubsystem* Physics  = nullptr;

        /** InSetup edits the config before physics starts (it is read at Startup). */
        explicit MotionFixture(const TFunction<void(EngineConfigData&)>& InSetup = {})
        {
            Config.Physics.Gravity = { 0.f, 0.f };
            if (InSetup) { InSetup(Config); }

            TheWorld = Worlds.CreateWorld("Motion", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Input, Config, /*Actions*/ nullptr, /*UI*/ nullptr, Debug });
            TheWorld->GetSubsystems().RegisterSubsystem<PhysicsSubsystem>(std::ref(*TheWorld->GetContext()));
            TheWorld->GetSubsystems().StartupAll();

            Physics = TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>();
            REQUIRE(Physics != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        void Steps(const Uint32 InCount)
        {
            for (Uint32 lStep = 0; lStep < InCount; ++lStep)
            {
                Worlds.Update(STEP);
                Worlds.FixedUpdate(STEP);
            }
        }

        /** A 50 x 50 box at InPosition; no Rigidbody means static. */
        Entity Box(const Vector2F& InPosition, const bool bInRigidbody, const EBodyType InType = EBodyType::Dynamic)
        {
            Entity lEntity = TheWorld->CreateEntity("Box");
            lEntity.Get<TransformComponent>().Position = InPosition;

            ColliderComponent lCollider;
            lCollider.Shape = EColliderShape::Box;
            lCollider.Size  = { 50.f, 50.f };
            lEntity.AddOrReplace<ColliderComponent>(lCollider);

            if (bInRigidbody)
            {
                RigidbodyComponent lBody;
                lBody.Type = InType;
                lEntity.AddOrReplace<RigidbodyComponent>(lBody);
            }

            return lEntity;
        }
    };

    Vector2F PositionOf(Entity& InEntity) { return InEntity.Get<TransformComponent>().Position; }
}

// =============================================================================
// Linear motion
// =============================================================================
TEST_CASE("PhysicsMotion: a body launched on the frame it is created moves at that velocity")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, /*bInRigidbody*/true);

    // No step has run yet: the body is built by the call.
    CHECK(lFix.Physics->GetBodyCount() == 0);
    REQUIRE(lFix.Physics->SetLinearVelocity(lBox.GetHandle(), { 120.f, 0.f }));
    CHECK(lFix.Physics->GetBodyCount() == 1);

    lFix.Steps(60);   // one second

    CHECK(PositionOf(lBox).x == doctest::Approx(120.f).epsilon(0.01));
    CHECK(PositionOf(lBox).y == doctest::Approx(0.f));
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(120.f));
    CHECK(lFix.Physics->GetBodyCount() == 1);   // the reconcile did not build a second one
}

TEST_CASE("PhysicsMotion: a body created this frame reads its mass and velocity at once")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, true);

    // Read first, before any step or launch: the getters build the body too.
    CHECK(lFix.Physics->GetBodyCount() == 0);
    CHECK(lFix.Physics->GetMass(lBox.GetHandle()) > 0.f);
    CHECK(lFix.Physics->GetBodyCount() == 1);
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(0.f));
    CHECK(lFix.Physics->GetAngularVelocity(lBox.GetHandle()) == doctest::Approx(0.f));
}

TEST_CASE("PhysicsMotion: an impulse changes the velocity by impulse / mass, at once")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, true);
    REQUIRE(lFix.Physics->HasDynamicBody(lBox.GetHandle()));

    const float lMass = lFix.Physics->GetMass(lBox.GetHandle());
    REQUIRE(lMass > 0.f);

    REQUIRE(lFix.Physics->ApplyImpulse(lBox.GetHandle(), { lMass * 10.f, lMass * -4.f }));
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(10.f));
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).y == doctest::Approx(-4.f));
}

TEST_CASE("PhysicsMotion: a force accelerates the body over one step, then is gone")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, true);
    REQUIRE(lFix.Physics->HasDynamicBody(lBox.GetHandle()));
    const float lMass = lFix.Physics->GetMass(lBox.GetHandle());

    // a = F / m = 600, over 1/60 s: +10.
    REQUIRE(lFix.Physics->ApplyForce(lBox.GetHandle(), { lMass * 600.f, 0.f }));
    lFix.Steps(1);
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(10.f).epsilon(0.01));

    // Not applied again: the speed stays.
    lFix.Steps(5);
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(10.f).epsilon(0.01));
}

// =============================================================================
// Rotation
// =============================================================================
TEST_CASE("PhysicsMotion: angular velocity in degrees per second turns the Transform")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, true);

    REQUIRE(lFix.Physics->SetAngularVelocity(lBox.GetHandle(), 90.f));
    CHECK(lFix.Physics->GetAngularVelocity(lBox.GetHandle()) == doctest::Approx(90.f));

    lFix.Steps(60);
    CHECK(lBox.Get<TransformComponent>().Rotation == doctest::Approx(90.f).epsilon(0.01));

    SUBCASE("torque and angular impulse turn it the same way (counter-clockwise for positive)")
    {
        Entity lOther = lFix.Box({ 500.f, 0.f }, true);

        REQUIRE(lFix.Physics->ApplyAngularImpulse(lOther.GetHandle(), 1.0e6f));
        CHECK(lFix.Physics->GetAngularVelocity(lOther.GetHandle()) > 0.f);

        const float lBefore = lFix.Physics->GetAngularVelocity(lOther.GetHandle());
        REQUIRE(lFix.Physics->ApplyTorque(lOther.GetHandle(), 1.0e8f));
        lFix.Steps(1);
        CHECK(lFix.Physics->GetAngularVelocity(lOther.GetHandle()) > lBefore);
    }
}

// =============================================================================
// Who may be driven
// =============================================================================
TEST_CASE("PhysicsMotion: only dynamic bodies take velocities, impulses and forces")
{
    MotionFixture lFix;
    Entity lStatic    = lFix.Box({ 0.f, 0.f }, /*bInRigidbody*/false);
    Entity lKinematic = lFix.Box({ 200.f, 0.f }, true, EBodyType::Kinematic);

    Entity lNoCollider = lFix.TheWorld->CreateEntity("NoCollider");
    lNoCollider.AddOrReplace<RigidbodyComponent>(RigidbodyComponent{});

    for (Entity lEntity : { lStatic, lKinematic, lNoCollider })
    {
        CHECK_FALSE(lFix.Physics->SetLinearVelocity(lEntity.GetHandle(), { 1.f, 0.f }));
        CHECK_FALSE(lFix.Physics->ApplyImpulse(lEntity.GetHandle(), { 1.f, 0.f }));
        CHECK_FALSE(lFix.Physics->ApplyForce(lEntity.GetHandle(), { 1.f, 0.f }));
        CHECK_FALSE(lFix.Physics->SetAngularVelocity(lEntity.GetHandle(), 1.f));
        CHECK_FALSE(lFix.Physics->ApplyTorque(lEntity.GetHandle(), 1.f));
        CHECK_FALSE(lFix.Physics->ApplyAngularImpulse(lEntity.GetHandle(), 1.f));
        CHECK(lFix.Physics->GetMass(lEntity.GetHandle()) == 0.f);
        CHECK(lFix.Physics->GetLinearVelocity(lEntity.GetHandle()).x == 0.f);
    }

    // A destroyed entity, too.
    Entity lGone = lFix.Box({ 400.f, 0.f }, true);
    const EntityID lGoneId = lGone.GetHandle();
    lFix.TheWorld->DestroyEntity(lGone);
    CHECK_FALSE(lFix.Physics->SetLinearVelocity(lGoneId, { 1.f, 0.f }));
}

// =============================================================================
// Who owns the pose
// =============================================================================
TEST_CASE("PhysicsMotion: moving a dynamic body's Transform teleports it and keeps its velocity")
{
    MotionFixture lFix;
    Entity lBox = lFix.Box({ 0.f, 0.f }, true);
    REQUIRE(lFix.Physics->SetLinearVelocity(lBox.GetHandle(), { 60.f, 0.f }));
    lFix.Steps(1);

    lBox.Get<TransformComponent>().Position = { 500.f, 100.f };
    lFix.Steps(1);

    // Teleported, then one step at 60 units/s.
    CHECK(PositionOf(lBox).x == doctest::Approx(501.f).epsilon(0.001));
    CHECK(PositionOf(lBox).y == doctest::Approx(100.f));
    CHECK(lFix.Physics->GetLinearVelocity(lBox.GetHandle()).x == doctest::Approx(60.f));
}

TEST_CASE("PhysicsMotion: a kinematic body follows its Transform")
{
    MotionFixture lFix;
    Entity lPlatform = lFix.Box({ 0.f, 0.f }, true, EBodyType::Kinematic);
    lFix.Steps(1);

    // A ray straight down at x = 300 finds nothing yet.
    const auto lProbe = [&lFix]() { return lFix.Physics->RayCast({ 300.f, 200.f }, { 0.f, -1.f }, 400.f); };
    CHECK_FALSE(lProbe().bHit);

    lPlatform.Get<TransformComponent>().Position = { 300.f, 0.f };
    lFix.Steps(1);

    const PhysicsSubsystem::RaycastHit lHit = lProbe();
    CHECK(lHit.bHit);
    CHECK(lHit.Entity == lPlatform.GetHandle());

    // It stopped there: no velocity left over from the move.
    lFix.Steps(10);
    CHECK(lProbe().bHit);
    CHECK_FALSE(lFix.Physics->RayCast({ 330.f, 200.f }, { 0.f, -1.f }, 400.f).bHit);
}

TEST_CASE("PhysicsMotion: a kinematic body moved further than one step can sweep lands there at once")
{
    MotionFixture lFix;
    Entity lPlatform = lFix.Box({ 0.f, 0.f }, true, EBodyType::Kinematic);
    lFix.Steps(1);

    // Far beyond the backend's speed cap for one step: swept, it would stop short.
    const float lFar = lFix.Physics->GetPhysicsWorld()->GetMaxLinearSpeed() * static_cast<float>(STEP) * 5.f;
    lPlatform.Get<TransformComponent>().Position = { lFar, 0.f };
    lFix.Steps(1);

    const PhysicsSubsystem::RaycastHit lHit = lFix.Physics->RayCast({ lFar, 200.f }, { 0.f, -1.f }, 400.f);
    CHECK(lHit.bHit);
    CHECK(lHit.Entity == lPlatform.GetHandle());
}

// =============================================================================
// Re-entrancy
// =============================================================================
TEST_CASE("PhysicsMotion: a world-bounds handler may create and launch a body")
{
    MotionFixture lFix([](EngineConfigData& InConfig)
    {
        InConfig.Physics.WorldBounds.bEnabled = true;
        InConfig.Physics.WorldBounds.Min      = { -1000.f, -1000.f };
        InConfig.Physics.WorldBounds.Max      = {  1000.f,  1000.f };
        InConfig.Physics.WorldBounds.Response = EWorldBoundsResponse::EventAndDestroy;
    });

    Entity lRunner = lFix.Box({ 990.f, 0.f }, true);
    REQUIRE(lFix.Physics->SetLinearVelocity(lRunner.GetHandle(), { 1200.f, 0.f }));

    Entity lReplacement;
    const DelegateHandle lHandle = lFix.Events.GetEventBus().Subscribe<PhysicsExitedWorldBounds>(
        [&lFix, &lReplacement](const PhysicsExitedWorldBounds&)
        {
            // Builds a body while the subsystem is still inside its bounds pass.
            lReplacement = lFix.Box({ 0.f, 0.f }, true);
            lFix.Physics->SetLinearVelocity(lReplacement.GetHandle(), { -50.f, 0.f });
        });

    lFix.Steps(5);

    CHECK_FALSE(lRunner.IsValid());
    REQUIRE(lReplacement.IsValid());
    CHECK(lFix.Physics->GetLinearVelocity(lReplacement.GetHandle()).x == doctest::Approx(-50.f));

    lFix.Events.GetEventBus().Unsubscribe(lHandle);
}
