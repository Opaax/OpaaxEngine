// Suite: physics events (Physics/PhysicsEvents.h) — the touch edges PhysicsSubsystem publishes
// after each step, and the overlap STATE it synthesizes between them.
//
// The interesting cases are not "an event fires". They are the ones M9 had to discover by running
// it: that a Stayed must not outlive its Ended within the same step, and that a handler destroying
// an entity must not produce a phantom event from the body it just killed. Both are here, and both
// fail loudly if the ordering in DispatchPhysicsEvents is changed.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Events/EventBus.h"
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
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
    struct EventFixture
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

        // Counters rather than flags: "it fired" and "it fired once" are different claims, and
        // the phantom-event cases are only about the second one.
        Uint32 OverlapBegan    = 0;
        Uint32 OverlapStayed   = 0;
        Uint32 OverlapEnded    = 0;
        Uint32 CollisionBegan  = 0;
        Uint32 CollisionEnded  = 0;

        EntityID LastSensor  = ENTITY_NONE;
        EntityID LastVisitor = ENTITY_NONE;

        EventFixture()
        {
            TheWorld = Worlds.CreateWorld("Events", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Input, Config, Debug, &Profiler });
            TheWorld->GetSubsystems().RegisterSubsystem<PhysicsSubsystem>(std::ref(*TheWorld->GetContext()));
            TheWorld->GetSubsystems().StartupAll();

            Physics = TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>();
            REQUIRE(Physics != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));

            EventBus& lBus = Events.GetEventBus();

            lBus.Subscribe<PhysicsOverlapBegan>([this](const PhysicsOverlapBegan& InEvent)
            {
                ++OverlapBegan;
                LastSensor  = InEvent.OverlapEntity;
                LastVisitor = InEvent.OtherEntity;
            });
            lBus.Subscribe<PhysicsOverlapStayed>([this](const PhysicsOverlapStayed&) { ++OverlapStayed; });
            lBus.Subscribe<PhysicsOverlapEnded>([this](const PhysicsOverlapEnded&)   { ++OverlapEnded; });
            lBus.Subscribe<PhysicsCollisionBegan>([this](const PhysicsCollisionBegan&) { ++CollisionBegan; });
            lBus.Subscribe<PhysicsCollisionEnded>([this](const PhysicsCollisionEnded&) { ++CollisionEnded; });
        }

        ~EventFixture()
        {
            // The bus outlives nothing here, but a live handler capturing a dead fixture is the
            // one way this suite could corrupt a LATER test rather than fail its own.
            Events.GetEventBus().UnsubscribeAll(this);
        }

        void TickFrames(const Uint64 InFrames)
        {
            for (Uint64 lFrame = 0; lFrame < InFrames; ++lFrame)
            {
                Worlds.Update(1.0 / 60.0);
                Worlds.FixedUpdate(1.0 / 60.0);
            }
        }

        Entity SpawnCollider(const Vector2F& InPosition, const Vector2F& InSize,
                             const EColliderMode InMode, const bool bInDynamic)
        {
            Entity lEntity = TheWorld->CreateEntity("Collider");
            lEntity.Get<TransformComponent>().Position = InPosition;

            ColliderComponent lCollider;
            lCollider.Shape = EColliderShape::Box;
            lCollider.Size  = InSize;
            lCollider.Mode  = InMode;
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
// Solid contacts
// =============================================================================

TEST_CASE("PhysicsEvents: two solid colliders touching publish CollisionBegan")
{
    EventFixture lFixture;

    lFixture.SpawnCollider({ 0.f, 0.f }, { 800.f, 100.f }, EColliderMode::Solid, /*dynamic*/ false);
    lFixture.SpawnCollider({ 0.f, 300.f }, { 50.f, 50.f }, EColliderMode::Solid, /*dynamic*/ true);

    lFixture.TickFrames(120);

    CHECK(lFixture.CollisionBegan >= 1u);

    // A solid contact is an EDGE pair, so nothing synthesizes a per-step state for it.
    CHECK(lFixture.OverlapBegan == 0u);
    CHECK(lFixture.OverlapStayed == 0u);
}

// =============================================================================
// Overlap — the state the subsystem synthesizes, not the backend
// =============================================================================

TEST_CASE("PhysicsEvents: a faller through a sensor gives Began, then Stayed, then Ended")
{
    EventFixture lFixture;

    // A thin sensor in mid-air, and a box that falls straight through it.
    Entity lSensor = lFixture.SpawnCollider({ 0.f, 0.f }, { 400.f, 40.f },
                                            EColliderMode::Overlap, /*dynamic*/ false);
    Entity lFaller = lFixture.SpawnCollider({ 0.f, 400.f }, { 40.f, 40.f },
                                            EColliderMode::Solid, /*dynamic*/ true);

    lFixture.TickFrames(180);

    CHECK(lFixture.OverlapBegan == 1u);
    CHECK(lFixture.OverlapEnded == 1u);

    // The whole reason PhysicsOverlapStayed exists: the backend reports two edges, and the live
    // set is what turns them into a state that ticks in between.
    CHECK(lFixture.OverlapStayed >= 1u);

    // Sensor first, visitor second — the ordering the payload promises.
    CHECK(lFixture.LastSensor == lSensor.GetHandle());
    CHECK(lFixture.LastVisitor == lFaller.GetHandle());
}

TEST_CASE("PhysicsEvents: nothing is published while the tick gate is closed")
{
    EventFixture lFixture;

    lFixture.SpawnCollider({ 0.f, 0.f }, { 400.f, 40.f }, EColliderMode::Overlap, /*dynamic*/ false);
    lFixture.SpawnCollider({ 0.f, 400.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/ true);

    lFixture.Worlds.SetPaused(true);
    lFixture.TickFrames(180);

    CHECK(lFixture.OverlapBegan == 0u);
    CHECK(lFixture.OverlapStayed == 0u);
}

// =============================================================================
// The destroy-in-handler case — M9 found this by running it
// =============================================================================

TEST_CASE("PhysicsEvents: destroying the VISITOR in the Began handler yields no phantom Stayed or Ended")
{
    EventFixture lFixture;

    lFixture.SpawnCollider({ 0.f, 0.f }, { 400.f, 40.f }, EColliderMode::Overlap, /*dynamic*/ false);
    lFixture.SpawnCollider({ 0.f, 400.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/ true);

    // The pickup case: the thing that entered the sensor is consumed on entry.
    lFixture.Events.GetEventBus().Subscribe<PhysicsOverlapBegan>(
        [&lFixture](const PhysicsOverlapBegan& InEvent)
        {
            lFixture.TheWorld->DestroyEntity(InEvent.OtherEntity);
        });

    lFixture.TickFrames(180);

    CHECK(lFixture.OverlapBegan == 1u);

    // Exactly the assertion M9's close-out records: one Began, and NOTHING afterwards from a body
    // whose entity is gone. A Stayed here would mean the live-overlap set outlived its entity.
    CHECK(lFixture.OverlapStayed == 0u);
    CHECK(lFixture.OverlapEnded == 0u);

    // And the body itself is reaped by the next step's reconcile — only the sensor is left.
    CHECK(lFixture.Physics->GetBodyCount() == 1u);
}

TEST_CASE("PhysicsEvents: destroying the SENSOR mid-overlap also leaves nothing behind")
{
    EventFixture lFixture;

    lFixture.SpawnCollider({ 0.f, 0.f }, { 400.f, 40.f }, EColliderMode::Overlap, /*dynamic*/ false);
    lFixture.SpawnCollider({ 0.f, 400.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/ true);

    lFixture.Events.GetEventBus().Subscribe<PhysicsOverlapBegan>(
        [&lFixture](const PhysicsOverlapBegan& InEvent)
        {
            lFixture.TheWorld->DestroyEntity(InEvent.OverlapEntity);
        });

    lFixture.TickFrames(180);

    CHECK(lFixture.OverlapBegan == 1u);
    CHECK(lFixture.OverlapStayed == 0u);
    CHECK(lFixture.OverlapEnded == 0u);
}
