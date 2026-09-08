// Suite: PhysicsSubsystem's queries and its kill volume (⑦-A P4).
//
// The seam already has RayCastClosest and OverlapAABB, and PhysicsSeamTests covers them there —
// in USER-DATA, which is meaningless to a caller. What is tested here is the resolution to
// EntityID and the two decisions that go with it: an unresolvable hit is reported as a MISS, and
// a query before Play is an empty answer rather than an error.
//
// The kill volume's interesting property is that it is LATCHED. "It fires" would pass for a
// version that fires sixty times a second, which is the version that is useless.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Events/EventBus.h"
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/PhysicsEvents.h"
#include "Renderer/DebugDraw.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/RigidbodyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/PhysicsSubsystem.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <functional>

using namespace Opaax;

namespace
{
    struct QueryFixture
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug;
        FrameProfiler    Profiler;
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*            TheWorld = nullptr;
        PhysicsSubsystem* Physics  = nullptr;

        Uint32   BoundsEvents = 0;
        EntityID LastExited   = ENTITY_NONE;
        Vector2F LastExitPos  = { 0.f, 0.f };

        /** Config is applied at Startup, so a fixture configures BEFORE it builds the world. */
        explicit QueryFixture(const WorldBoundsSettings& InBounds = {})
        {
            Config.Physics.WorldBounds = InBounds;

            TheWorld = Worlds.CreateWorld("Queries", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Input, Config, /*Actions*/ nullptr, Debug, &Profiler });
            TheWorld->GetSubsystems().RegisterSubsystem<PhysicsSubsystem>(std::ref(*TheWorld->GetContext()));
            TheWorld->GetSubsystems().StartupAll();

            Physics = TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>();
            REQUIRE(Physics != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));

            Events.GetEventBus().Subscribe<PhysicsExitedWorldBounds>(
                [this](const PhysicsExitedWorldBounds& InEvent)
                {
                    ++BoundsEvents;
                    LastExited  = InEvent.Entity;
                    LastExitPos = InEvent.LastPosition;
                });
        }

        ~QueryFixture() { Events.GetEventBus().UnsubscribeAll(this); }

        void TickFrames(const Uint64 InFrames)
        {
            for (Uint64 lFrame = 0; lFrame < InFrames; ++lFrame)
            {
                Worlds.Update(1.0 / 60.0);
                Worlds.FixedUpdate(1.0 / 60.0);
            }
        }

        Entity SpawnBox(const Vector2F& InPosition, const Vector2F& InSize, const bool bInDynamic,
                        const ECollisionChannel InChannel = ECollisionChannel::WorldStatic)
        {
            Entity lEntity = TheWorld->CreateEntity("Box");
            lEntity.Get<TransformComponent>().Position = InPosition;

            ColliderComponent lCollider;
            lCollider.Shape   = EColliderShape::Box;
            lCollider.Size    = InSize;
            lCollider.Channel = InChannel;
            lEntity.AddOrReplace<ColliderComponent>(lCollider);

            if (bInDynamic)
            {
                RigidbodyComponent lBody;
                lBody.Type = EBodyType::Dynamic;
                lEntity.AddOrReplace<RigidbodyComponent>(lBody);
            }

            return lEntity;
        }
    };
}

// =============================================================================
// RayCast — the seam's user-data, resolved
// =============================================================================

TEST_CASE("PhysicsSubsystem::RayCast resolves the hit to the OWNING ENTITY")
{
    QueryFixture lFixture;

    Entity lFloor = lFixture.SpawnBox({ 0.f, 0.f }, { 800.f, 100.f }, /*dynamic*/ false);
    lFixture.TickFrames(2);

    const PhysicsSubsystem::RaycastHit lHit =
        lFixture.Physics->RayCast({ 0.f, 400.f }, { 0.f, -1.f }, 1000.f);

    REQUIRE(lHit.bHit);

    // The whole point of this layer: the seam answers in user-data, a caller wants an entity.
    CHECK(lHit.Entity == lFloor.GetHandle());
    CHECK(lHit.Point.y == doctest::Approx(50.f).epsilon(0.05));   // the floor's top surface
    CHECK(lHit.Normal.y > 0.9f);
}

TEST_CASE("PhysicsSubsystem::RayCast into empty space misses, and reports ENTITY_NONE")
{
    QueryFixture lFixture;

    lFixture.SpawnBox({ 0.f, 0.f }, { 800.f, 100.f }, /*dynamic*/ false);
    lFixture.TickFrames(2);

    const PhysicsSubsystem::RaycastHit lHit =
        lFixture.Physics->RayCast({ 0.f, 400.f }, { 0.f, 1.f }, 1000.f);   // upward, away

    CHECK_FALSE(lHit.bHit);
    CHECK(lHit.Entity == ENTITY_NONE);
}

TEST_CASE("PhysicsSubsystem::RayCast honours the CHANNEL mask")
{
    QueryFixture lFixture;

    lFixture.SpawnBox({ 0.f, 0.f }, { 800.f, 100.f }, /*dynamic*/ false, ECollisionChannel::Pawn);
    lFixture.TickFrames(2);

    const PhysicsSubsystem::RaycastHit lOnPawn =
        lFixture.Physics->RayCast({ 0.f, 400.f }, { 0.f, -1.f }, 1000.f,
                                  CategoryBit(ECollisionChannel::Pawn));
    CHECK(lOnPawn.bHit);

    // Same ray, a mask the collider's channel is not in: the shape is invisible to it.
    const PhysicsSubsystem::RaycastHit lOnProjectile =
        lFixture.Physics->RayCast({ 0.f, 400.f }, { 0.f, -1.f }, 1000.f,
                                  CategoryBit(ECollisionChannel::Projectile));
    CHECK_FALSE(lOnProjectile.bHit);
}

// =============================================================================
// OverlapAABB
// =============================================================================

TEST_CASE("PhysicsSubsystem::OverlapAABB answers in entities, and clears the output first")
{
    QueryFixture lFixture;

    Entity lBox = lFixture.SpawnBox({ 0.f, 0.f }, { 100.f, 100.f }, /*dynamic*/ false);
    lFixture.TickFrames(2);

    TDynArray<EntityID> lHits;
    lHits.push_back(ENTITY_NONE);   // stale content from a previous query must not survive

    lFixture.Physics->OverlapAABB({ -60.f, -60.f }, { 60.f, 60.f }, lHits);

    REQUIRE(lHits.size() == 1u);
    CHECK(lHits[0] == lBox.GetHandle());

    // Empty region: still cleared, so a caller never reads last query's answer.
    lFixture.Physics->OverlapAABB({ 9000.f, 9000.f }, { 9100.f, 9100.f }, lHits);
    CHECK(lHits.empty());
}

// =============================================================================
// The kill volume — the interesting property is that it LATCHES
// =============================================================================

TEST_CASE("PhysicsSubsystem: world bounds are OFF by default, so nothing is ever reaped")
{
    QueryFixture lFixture;   // default settings

    lFixture.SpawnBox({ 0.f, 0.f }, { 50.f, 50.f }, /*dynamic*/ true);
    lFixture.TickFrames(300);   // five seconds of falling, far past any sane bound

    CHECK(lFixture.BoundsEvents == 0u);
    CHECK(lFixture.Physics->GetBodyCount() == 1u);
}

TEST_CASE("PhysicsSubsystem: a body leaving the bounds fires ONCE and is reaped")
{
    WorldBoundsSettings lBounds;
    lBounds.bEnabled = true;
    lBounds.Min      = { -500.f, -300.f };
    lBounds.Max      = {  500.f,  1000.f };
    lBounds.Response = EWorldBoundsResponse::EventAndDestroy;

    QueryFixture lFixture(lBounds);

    Entity lFaller = lFixture.SpawnBox({ 0.f, 0.f }, { 50.f, 50.f }, /*dynamic*/ true);
    const EntityID lHandle = lFaller.GetHandle();

    lFixture.TickFrames(180);

    // ONCE, not once per step it kept falling — which is the difference between usable and not.
    CHECK(lFixture.BoundsEvents == 1u);
    CHECK(lFixture.LastExited == lHandle);
    CHECK(lFixture.LastExitPos.y < -300.f);

    // Reaped: the entity is gone and so is its body.
    CHECK_FALSE(lFixture.TheWorld->IsValid(lHandle));
    CHECK(lFixture.Physics->GetBodyCount() == 0u);
}

TEST_CASE("PhysicsSubsystem: EventOnly fires the event and leaves the entity ALONE")
{
    WorldBoundsSettings lBounds;
    lBounds.bEnabled = true;
    lBounds.Min      = { -500.f, -300.f };
    lBounds.Max      = {  500.f,  1000.f };
    lBounds.Response = EWorldBoundsResponse::EventOnly;

    QueryFixture lFixture(lBounds);

    Entity lFaller = lFixture.SpawnBox({ 0.f, 0.f }, { 50.f, 50.f }, /*dynamic*/ true);
    const EntityID lHandle = lFaller.GetHandle();

    lFixture.TickFrames(180);

    CHECK(lFixture.BoundsEvents == 1u);

    // The game reacts, the engine does not: still alive, still simulated, still falling.
    CHECK(lFixture.TheWorld->IsValid(lHandle));
    CHECK(lFixture.Physics->GetBodyCount() == 1u);

    // And it STAYS at one event however far it falls — the latch is what EventOnly leans on.
    lFixture.TickFrames(180);
    CHECK(lFixture.BoundsEvents == 1u);
}

TEST_CASE("PhysicsSubsystem: a STATIC collider outside the bounds is left alone")
{
    WorldBoundsSettings lBounds;
    lBounds.bEnabled = true;
    lBounds.Min      = { -100.f, -100.f };
    lBounds.Max      = {  100.f,  100.f };
    lBounds.Response = EWorldBoundsResponse::EventAndDestroy;

    QueryFixture lFixture(lBounds);

    // Authored far outside, deliberately: reaping it would delete level geometry someone placed.
    Entity lFarGeometry = lFixture.SpawnBox({ 5000.f, 5000.f }, { 100.f, 100.f }, /*dynamic*/ false);

    lFixture.TickFrames(60);

    CHECK(lFixture.BoundsEvents == 0u);
    CHECK(lFixture.TheWorld->IsValid(lFarGeometry.GetHandle()));
}

TEST_CASE("PhysicsSubsystem: a query before Play answers empty rather than failing")
{
    // A world with no physics subsystem at all is the Edit case; here the subsystem exists but is
    // asked to answer with no world, which is what Shutdown leaves behind.
    QueryFixture lFixture;
    lFixture.TheWorld->GetSubsystems().ShutdownAll();

    const PhysicsSubsystem::RaycastHit lHit =
        lFixture.Physics->RayCast({ 0.f, 0.f }, { 1.f, 0.f }, 100.f);
    CHECK_FALSE(lHit.bHit);

    TDynArray<EntityID> lHits;
    lHits.push_back(ENTITY_NONE);
    lFixture.Physics->OverlapAABB({ -10.f, -10.f }, { 10.f, 10.f }, lHits);
    CHECK(lHits.empty());
}
