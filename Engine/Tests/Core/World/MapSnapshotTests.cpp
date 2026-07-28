// Suite: the M3 snapshot core — MapSerializer::Capture <-> MapFactory::Instantiate.
//
// The milestone gate lives here: capture -> instantiate must yield an equivalent world with
// GUIDs PRESERVED. entt handles are runtime-only and are never assumed stable across worlds,
// so the Guid is the only thing an inter-entity reference can survive on — if the round trip
// re-mints identities, every such reference in a map silently points at the wrong entity.
#include <doctest.h>

#include "World/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    // A component defined exe-side, as a game module's would be.
    struct StatsComponent
    {
        int   Health = 0;
        float Speed  = 0.f;

        bool operator==(const StatsComponent& InOther) const
        {
            return Health == InOther.Health && Speed == InOther.Speed;
        }
    };

    inline void to_json(nlohmann::json& InJson, const StatsComponent& InValue)
    {
        InJson = nlohmann::json{{"Health", InValue.Health}, {"Speed", InValue.Speed}};
    }

    inline void from_json(const nlohmann::json& InJson, StatsComponent& InValue)
    {
        InJson.at("Health").get_to(InValue.Health);
        InJson.at("Speed").get_to(InValue.Speed);
    }

    // A registry carrying both an engine-side and a test-side component type.
    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<DummyComponent>("Dummy"));
        REQUIRE(InRegistry.Register<StatsComponent>("Stats"));
    }
}

// =============================================================================
// The gate
// =============================================================================
TEST_CASE("Snapshot: capture -> clear -> instantiate rebuilds an equivalent world, GUIDs preserved")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMap = MapId("Level01");

    World lWorld("RoundTrip");

    Entity lHero = lWorld.CreateEntity("Hero", lMap);
    lHero.Add<StatsComponent>(StatsComponent{100, 4.5f});
    lHero.Add<DummyComponent>();
    lHero.Get<DummyComponent>().Position = Vector2F{12.f, -3.f};

    Entity lCrate = lWorld.CreateEntity("Crate", lMap);
    lCrate.Add<DummyComponent>();

    const Guid lHeroGuid  = lHero.GetGuid();
    const Guid lCrateGuid = lCrate.GetGuid();

    const MapData lCaptured = MapSerializer::Capture(lWorld, lRegistry);
    REQUIRE(lCaptured.EntityCount() == 2u);

    // Wipe the world completely — the entt handles from before are now meaningless, which is
    // exactly the condition instantiate has to survive.
    lWorld.Clear();
    REQUIRE(lWorld.GetEntityCount() == 0u);

    CHECK(MapFactory::Instantiate(lCaptured, lWorld, lRegistry) == 2u);
    CHECK(lWorld.GetEntityCount() == 2u);

    // Identity survived: the SAME Guids resolve in the rebuilt world.
    Entity lNewHero = lWorld.FindByGuid(lHeroGuid);
    REQUIRE(lNewHero.IsValid());
    CHECK(lNewHero.Get<EntityMeta>().Name == "Hero");
    CHECK(lNewHero.Get<EntityMeta>().OwnerMap == lMap);

    // Component values survived, both the exe-side type and the engine-side one.
    REQUIRE(lNewHero.Has<StatsComponent>());
    CHECK(lNewHero.Get<StatsComponent>() == StatsComponent{100, 4.5f});
    REQUIRE(lNewHero.Has<DummyComponent>());
    CHECK(lNewHero.Get<DummyComponent>().Position.x == doctest::Approx(12.f));
    CHECK(lNewHero.Get<DummyComponent>().Position.y == doctest::Approx(-3.f));

    Entity lNewCrate = lWorld.FindByGuid(lCrateGuid);
    REQUIRE(lNewCrate.IsValid());
    CHECK(lNewCrate.Has<DummyComponent>());
    CHECK_FALSE(lNewCrate.Has<StatsComponent>()); // it never had one
}

TEST_CASE("Snapshot: a round trip into a DIFFERENT world preserves identity too")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World  lSource("Source");
    Entity lEntity = lSource.CreateEntity("Traveller", MapId("Level01"));
    lEntity.Add<StatsComponent>(StatsComponent{7, 1.5f});

    const Guid lGuid = lEntity.GetGuid();

    // This is the PIE clone shape (M4): capture one world, instantiate into another.
    World lTarget("Target");
    CHECK(MapFactory::Instantiate(MapSerializer::Capture(lSource, lRegistry), lTarget, lRegistry) == 1u);

    Entity lClone = lTarget.FindByGuid(lGuid);
    REQUIRE(lClone.IsValid());
    CHECK(lClone.Get<StatsComponent>() == StatsComponent{7, 1.5f});

    // The two worlds are independent: entt handles are per-registry, so editing the clone
    // must not touch the original.
    lClone.Get<StatsComponent>().Health = 999;
    CHECK(lSource.FindByGuid(lGuid).Get<StatsComponent>().Health == 7);
}

// =============================================================================
// Filtering — the Map partition
// =============================================================================
TEST_CASE("Snapshot: a filtered capture returns only the entities that map authored")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMapA = MapId("Level01");
    const MapId lMapB = MapId("Level02");

    World lWorld("TwoMaps");
    lWorld.CreateEntity("A1", lMapA);
    lWorld.CreateEntity("A2", lMapA);
    lWorld.CreateEntity("B1", lMapB);

    const MapData lOnlyA = MapSerializer::Capture(lWorld, lRegistry, lMapA);

    REQUIRE(lOnlyA.EntityCount() == 2u);
    for (const EntityData& lEntityData : lOnlyA.Entities)
    {
        CHECK(lEntityData.OwnerMap == lMapA);
    }
}

TEST_CASE("Snapshot: a filtered capture excludes runtime-spawned entities")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMap = MapId("Level01");

    World lWorld("Playing");
    lWorld.CreateEntity("AuthoredCrate", lMap);

    // Mid-play spawns — no map authored these, so no map may save them. Without this rule,
    // saving during play writes a thousand bullets into the map file.
    lWorld.CreateEntity("Bullet");
    lWorld.CreateEntity("Bullet");
    lWorld.CreateEntity("Explosion");

    const MapData lSaved = MapSerializer::Capture(lWorld, lRegistry, lMap);

    REQUIRE(lSaved.EntityCount() == 1u);
    CHECK(lSaved.Entities[0].Name == "AuthoredCrate");
}

TEST_CASE("Snapshot: an UNfiltered capture takes the whole world, runtime spawns included")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Playing");
    lWorld.CreateEntity("Authored", MapId("Level01"));
    lWorld.CreateEntity("Bullet");

    // No filter means "snapshot everything" — the PIE clone case, where dropping live runtime
    // state would make the clone diverge from the world it copied.
    CHECK(MapSerializer::Capture(lWorld, lRegistry).EntityCount() == 2u);
}

TEST_CASE("Snapshot: capturing an empty world yields empty data, not a crash")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Empty");

    const MapData lData = MapSerializer::Capture(lWorld, lRegistry);
    CHECK(lData.IsEmpty());
    CHECK(lData.EntityCount() == 0u);
}

// =============================================================================
// What the registry decides
// =============================================================================
TEST_CASE("Snapshot: an UNREGISTERED component type is not captured")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<DummyComponent>("Dummy")); // StatsComponent deliberately absent

    World  lWorld("Partial");
    Entity lEntity = lWorld.CreateEntity("Subject", MapId("Level01"));
    lEntity.Add<DummyComponent>();
    lEntity.Add<StatsComponent>(StatsComponent{50, 2.f});

    const MapData lData = MapSerializer::Capture(lWorld, lRegistry);

    REQUIRE(lData.EntityCount() == 1u);
    REQUIRE(lData.Entities[0].Components.size() == 1u);
    CHECK(lData.Entities[0].Components[0].TypeName == OpaaxStringID("Dummy"));
}

TEST_CASE("Snapshot: an unknown component name is skipped, and the entity still loads")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<DummyComponent>("Dummy"));

    // Hand-built data standing in for a map written by a build that knew one more type.
    MapData lData;
    EntityData lEntityData;
    lEntityData.Id       = Guid::New();
    lEntityData.Name     = "FromTheFuture";
    lEntityData.OwnerMap = MapId("Level01");
    lEntityData.Components.push_back(ComponentData{OpaaxStringID("Dummy"), nlohmann::json(DummyComponent{})});
    lEntityData.Components.push_back(ComponentData{OpaaxStringID("NotInThisBuild"), nlohmann::json{{"x", 1}}});
    lData.Entities.push_back(Move(lEntityData));

    World lWorld("ForwardCompat");

    // Forward compatibility: losing one component beats refusing to open the map at all.
    CHECK(MapFactory::Instantiate(lData, lWorld, lRegistry) == 1u);

    Entity lEntity = lWorld.FindByGuid(lData.Entities[0].Id);
    REQUIRE(lEntity.IsValid());
    CHECK(lEntity.Has<DummyComponent>());
}

// =============================================================================
// Instantiate's contract
// =============================================================================
TEST_CASE("Snapshot: Instantiate is ADDITIVE — it does not clear the world first")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorldA("MapA");
    lWorldA.CreateEntity("A1", MapId("Level01"));
    const MapData lMapA = MapSerializer::Capture(lWorldA, lRegistry);

    World lTarget("Streaming");
    lTarget.CreateEntity("AlreadyHere", MapId("Root"));

    // Streaming a second map in must not wipe the first — a factory that cleared could
    // never serve level streaming.
    CHECK(MapFactory::Instantiate(lMapA, lTarget, lRegistry) == 1u);
    CHECK(lTarget.GetEntityCount() == 2u);
}

TEST_CASE("Snapshot: instantiating the same data twice refuses the duplicates")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Source");
    lWorld.CreateEntity("Unique", MapId("Level01"));

    const MapData lData = MapSerializer::Capture(lWorld, lRegistry);

    World lTarget("Target");
    CHECK(MapFactory::Instantiate(lData, lTarget, lRegistry) == 1u);

    // Same GUIDs, already live: every entity is refused. The count is what discriminates —
    // a silent second copy would give FindByGuid two candidates.
    CHECK(MapFactory::Instantiate(lData, lTarget, lRegistry) == 0u);
    CHECK(lTarget.GetEntityCount() == 1u);
}
