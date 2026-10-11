// Suite: PhysicsSubsystem — owns the physics world and keeps one body per collider.
// Runs headless, driven through WorldManager (the real tick path). Every entity is spawned after
// StartupAll, like in a real host, so the reconcile is what picks them up.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Resources/ResourceManager.h"
#include "Renderer/DebugDraw.h"
#include "Physics/Components/ColliderComponent.h"
#include "Physics/Components/RigidbodyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "Physics/PhysicsSubsystem.h"
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
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*            TheWorld = nullptr;
        PhysicsSubsystem* Physics  = nullptr;

        explicit PhysicsFixture(EWorldMode InMode = EWorldMode::Play)
        {
            TheWorld = Worlds.CreateWorld("PhysicsTest", InMode);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Input, Config, /*Actions*/ nullptr, /*UI*/ nullptr, Debug});

            // std::ref: by value, the context would be copied into a factory that StartupAll destroys.
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
// Creation filter
// =============================================================================

TEST_CASE("PhysicsSubsystem: it is a PLAY-world subsystem and refuses an Edit world")
{
    World lPlay("Play", EWorldMode::Play);
    World lEdit("Edit", EWorldMode::Edit);

    CHECK(PhysicsSubsystem::ShouldCreate(lPlay));
    CHECK_FALSE(PhysicsSubsystem::ShouldCreate(lEdit));
}

// =============================================================================
// Reconciliation
// =============================================================================

TEST_CASE("PhysicsSubsystem: no bodies at Startup, and the first fixed step builds them")
{
    PhysicsFixture lFixture;

    // Nothing spawned yet.
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

    // It now gains a rigidbody: without the rebuild it would stay static forever.
    RigidbodyComponent lBody;
    lBody.Type = EBodyType::Dynamic;
    lEntity.AddOrReplace<RigidbodyComponent>(lBody);

    lFixture.TickFrames(30);

    CHECK(lFixture.Physics->GetBodyCount() == 1u);
    CHECK(lFixture.PositionY(lEntity) < 490.f);   // it is falling now
}

// =============================================================================
// It falls, and it lands
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

TEST_CASE("PhysicsSubsystem: a dynamic body on a CHILD simulates in world space and stores its LOCAL")
{
    PhysicsFixture lFixture;

    lFixture.SpawnBox(0.f, { 1000.f, 100.f }, /*dynamic*/ false);   // floor top at y = 50

    // A holder at (300, 0), and a faller hung under it at LOCAL (0, 500) — world (300, 500).
    Entity lHolder = lFixture.TheWorld->CreateEntity("Holder");
    lHolder.Get<TransformComponent>().Position = { 300.f, 0.f };

    Entity lFaller = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);
    lFaller.Get<RigidbodyComponent>().bFixedRotation = true;   // a tilted landing slides a hair
    REQUIRE(EntityHierarchy::SetParent(lFaller, lHolder, /*bKeepWorld*/false));

    lFixture.TickFrames(180);

    // It fell from WORLD (300, 500) and landed at world y = 75. The component holds the LOCAL that
    // puts it there — exactly the world pose minus the holder's — and the world answer is the
    // body's. (Box2D slides a landing box a unit or so in x; the relationship is what is asserted.)
    const TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lFaller);
    const TransformComponent lLocalXf = lFaller.Get<TransformComponent>();
    CHECK(lWorldXf.Position.x == doctest::Approx(300.f).epsilon(0.01));
    CHECK(lWorldXf.Position.y == doctest::Approx(75.f).epsilon(0.05));

    CHECK(lLocalXf.Position.x == doctest::Approx(lWorldXf.Position.x - 300.f));
    CHECK(lLocalXf.Position.y == doctest::Approx(lWorldXf.Position.y));
    CHECK(lFaller.Get<EntityMeta>().Parent == lHolder.GetGuid());
}

TEST_CASE("PhysicsSubsystem: the tick GATE stops the simulation, and resuming continues it")
{
    PhysicsFixture lFixture;

    Entity lFaller = lFixture.SpawnBox(500.f, { 50.f, 50.f }, /*dynamic*/ true);
    lFixture.TickFrames(10);

    const float lMoving = lFixture.PositionY(lFaller);
    CHECK(lMoving < 500.f);

    // The tick gate belongs to WorldManager; physics has none of its own.
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
