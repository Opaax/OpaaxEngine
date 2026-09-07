// Suite: PhysicsSubsystem (World/Systems/PhysicsSubsystem.h) — the world subsystem that owns the
// physics world and reconciles one body per collider.
//
// A World needs no GL context, so the whole subsystem runs headless here: the fixture is
// WorldTickGateTests' GatedWorld shape (context first, register with std::ref per WS4, then
// StartupAll), driven through WorldManager so the tick path under test is the REAL one —
// Update once, then the fixed steps it owes.
//
// This is what makes P1's claim gateable without eyes. WS7 says a world subsystem may not assume
// entities exist at Startup, and these cases prove the consequence rather than restating it:
// every entity below is spawned AFTER StartupAll, exactly like a host's PostEngineStartup or a
// PIE clone's Instantiate, and the reconcile is what picks them up.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/RigidbodyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"   // World.h only forward-declares it
#include "World/Systems/PhysicsSubsystem.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <functional>

using namespace Opaax;

namespace
{
    // A bare manager builds worlds with no subsystems (it has no engine to resolve a context
    // from), so physics is injected the way CreateSubsystemsFor would have.
    struct PhysicsFixture
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        FrameProfiler    Profiler;
        EngineConfigData Config;
        WorldManager     Worlds;

        World*            TheWorld = nullptr;
        PhysicsSubsystem* Physics  = nullptr;

        explicit PhysicsFixture(EWorldMode InMode = EWorldMode::Play)
        {
            TheWorld = Worlds.CreateWorld("PhysicsTest", InMode);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Config, Debug, &Profiler });

            // std::ref is load-bearing — WS4. By value, the context would be copied into a factory
            // lambda that StartupAll then destroys.
            TheWorld->GetSubsystems().RegisterSubsystem<PhysicsSubsystem>(std::ref(*TheWorld->GetContext()));
            TheWorld->GetSubsystems().StartupAll();

            Physics = TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>();
            REQUIRE(Physics != nullptr);
            REQUIRE(Physics->GetPhysicsWorld() != nullptr);

            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        /** One frame the way Engine::Loop drives it. */
        void TickFrames(const Uint64 InFrames)
        {
            for (Uint64 lFrame = 0; lFrame < InFrames; ++lFrame)
            {
                Worlds.Update(1.0 / 60.0);
                Worlds.FixedUpdate(1.0 / 60.0);
            }
        }

        /** A box collider at InY. With a rigidbody it falls; without one it is level geometry. */
        Entity SpawnBox(const float InY, const Vector2F& InSize, const bool bInDynamic)
        {
            Entity lEntity = TheWorld->CreateEntity("Box");

            lEntity.Get<TransformComponent>().Position = { 0.f, InY };

            ColliderComponent lCollider;
            lCollider.Shape = EColliderShape::Box;
            lCollider.Size  = InSize;
            lEntity.AddOrReplace<ColliderComponent>(lCollider);

            if (bInDynamic)
            {
                RigidbodyComponent lBody;
                lBody.Type = EBodyType::Dynamic;
                lEntity.AddOrReplace<RigidbodyComponent>(lBody);
            }

            return lEntity;
        }

        float PositionY(Entity& InEntity) const { return InEntity.Get<TransformComponent>().Position.y; }
    };
}

// =============================================================================
// Creation filter — the single line that replaces M9's whole PIE apparatus
// =============================================================================

TEST_CASE("PhysicsSubsystem: it is a PLAY-world subsystem and refuses an Edit world")
{
    World lPlay("Play", EWorldMode::Play);
    World lEdit("Edit", EWorldMode::Edit);

    CHECK(PhysicsSubsystem::ShouldCreate(lPlay));
    CHECK_FALSE(PhysicsSubsystem::ShouldCreate(lEdit));
}

// =============================================================================
// Reconciliation — WS7 in practice
// =============================================================================

TEST_CASE("PhysicsSubsystem: no bodies at Startup, and the first fixed step builds them")
{
    PhysicsFixture lFixture;

    // Nothing has been spawned yet, and nothing may have been assumed (WS7).
    CHECK(lFixture.Physics->GetBodyCount() == 0u);

    lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);

    // Still zero: spawning does not build, reconciling does.
    CHECK(lFixture.Physics->GetBodyCount() == 0u);

    lFixture.TickFrames(1);
    CHECK(lFixture.Physics->GetBodyCount() == 1u);
}

TEST_CASE("PhysicsSubsystem: a collider with no rigidbody becomes a STATIC body and never moves")
{
    PhysicsFixture lFixture;

    Entity lFloor = lFixture.SpawnBox(0.f, { 1000.f, 100.f }, /*dynamic*/ false);
    lFixture.TickFrames(60);

    CHECK(lFixture.Physics->GetBodyCount() == 1u);
    CHECK(lFixture.PositionY(lFloor) == doctest::Approx(0.f));
}

TEST_CASE("PhysicsSubsystem: a rigidbody with NO collider gets no body at all")
{
    PhysicsFixture lFixture;

    Entity lEntity = lFixture.TheWorld->CreateEntity("ShapelessBody");
    lEntity.AddOrReplace<RigidbodyComponent>(RigidbodyComponent{});

    lFixture.TickFrames(10);

    // A body with no shape is something nothing can touch, moved invisibly by gravity. Skipped.
    CHECK(lFixture.Physics->GetBodyCount() == 0u);
}

TEST_CASE("PhysicsSubsystem: a destroyed entity's body is reaped")
{
    PhysicsFixture lFixture;

    Entity lBox = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);
    lFixture.TickFrames(1);
    REQUIRE(lFixture.Physics->GetBodyCount() == 1u);

    lBox.Destroy();
    lFixture.TickFrames(1);

    CHECK(lFixture.Physics->GetBodyCount() == 0u);
}

TEST_CASE("PhysicsSubsystem: adding a Rigidbody AFTER the body exists rebuilds it, so add-order does not matter")
{
    PhysicsFixture lFixture;

    // Collider first: it becomes static, because that is what a collider alone means.
    Entity lEntity = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ false);
    lFixture.TickFrames(30);

    const float lStaticY = lFixture.PositionY(lEntity);
    CHECK(lStaticY == doctest::Approx(500.f));   // static: gravity does not touch it

    // Now it gains a rigidbody. Without the rebuild it would stay static forever — the exact
    // silent failure the BuiltType comparison exists to prevent.
    RigidbodyComponent lBody;
    lBody.Type = EBodyType::Dynamic;
    lEntity.AddOrReplace<RigidbodyComponent>(lBody);

    lFixture.TickFrames(30);

    CHECK(lFixture.Physics->GetBodyCount() == 1u);
    CHECK(lFixture.PositionY(lEntity) < 490.f);   // it is falling now
}

// =============================================================================
// The P1 gate: it falls, and it LANDS
// =============================================================================

TEST_CASE("PhysicsSubsystem: a dynamic box falls and comes to rest on static geometry")
{
    PhysicsFixture lFixture;

    // Floor top surface at y = 50 (centre 0, half-height 50).
    lFixture.SpawnBox(0.f, { 1000.f, 100.f }, /*dynamic*/ false);
    Entity lFaller = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);

    lFixture.TickFrames(180);   // three seconds

    CHECK(lFixture.Physics->GetBodyCount() == 2u);

    // Floor top 50 + the faller's half-height 25 = 75. Asserting only "it went down" would pass
    // for a box that fell through the floor forever, which is the failure worth catching.
    const float lRestY = lFixture.PositionY(lFaller);
    CHECK(lRestY == doctest::Approx(75.f).epsilon(0.05));

    // And it STOPPED — another second may not move it.
    lFixture.TickFrames(60);
    CHECK(lFixture.PositionY(lFaller) == doctest::Approx(lRestY).epsilon(0.01));
}

TEST_CASE("PhysicsSubsystem: the tick GATE stops the simulation, and resuming continues it")
{
    PhysicsFixture lFixture;

    Entity lFaller = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);
    lFixture.TickFrames(10);

    const float lMoving = lFixture.PositionY(lFaller);
    CHECK(lMoving < 500.f);

    // WS8: the gate is WorldManager's and physics holds none of its own.
    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrames(30);
    CHECK(lFixture.PositionY(lFaller) == doctest::Approx(lMoving));

    lFixture.Worlds.SetPaused(false);
    lFixture.TickFrames(10);
    CHECK(lFixture.PositionY(lFaller) < lMoving);
}

// =============================================================================
// Translation — the conversions that are silently wrong when they are wrong
// =============================================================================

TEST_CASE("PhysicsSubsystem: gravity comes from CONFIG, not from a hardcoded default")
{
    PhysicsFixture lFixture;

    // The fixture's config is default-constructed, so this pins the default rather than a literal
    // written twice.
    const Vector2F lGravity = lFixture.Physics->GetPhysicsWorld()->GetGravity();
    CHECK(lGravity.y == doctest::Approx(EngineConfigData{}.Physics.Gravity.y));
}

TEST_CASE("PhysicsSubsystem: a body's rotation round-trips DEGREES through a radians seam")
{
    PhysicsFixture lFixture;

    Entity lEntity = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);
    lEntity.Get<TransformComponent>().Rotation = 90.f;

    // Fixed rotation so the solver cannot change the angle: what comes back must be what went in,
    // and a missing conversion would read as ~5157 degrees (90 radians) or ~1.57 (90 taken as radians).
    RigidbodyComponent lBody;
    lBody.Type            = EBodyType::Dynamic;
    lBody.bFixedRotation  = true;
    lEntity.AddOrReplace<RigidbodyComponent>(lBody);

    lFixture.TickFrames(2);

    CHECK(lEntity.Get<TransformComponent>().Rotation == doctest::Approx(90.f).epsilon(0.01));
}
