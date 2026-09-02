// Suite: the RESTORE half of the snapshot core — MapSerializer::CaptureEntities <-> MapFactory::Restore.
//
// This is what the editor's undo replays, so the gate is the one the map format already trusts:
// capture a set of entities, edit the world, restore, capture again — and the two captures must
// serialize BYTE-FOR-BYTE the same (MP6's standard, applied to a subset rather than a file).
//
// Restore answers a different question from Instantiate and the difference is the point: Instantiate
// REFUSES a Guid that is already live, which is right for loading a map and wrong for putting one
// back. The cases below pin all three of Restore's jobs — recreate what is gone, overwrite what is
// there, and take off what the record does not name.
#include <doctest.h>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    // A component with values worth comparing, defined exe-side as a game module's would be.
    struct HealthComponent
    {
        int   Hp    = 0;
        float Armor = 0.f;

        bool operator==(const HealthComponent& InOther) const
        {
            return Hp == InOther.Hp && Armor == InOther.Armor;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HealthComponent, Hp, Armor)
    };

    void FillRegistry(ComponentRegistry& InRegistry, const bool bInTransformEssential = false)
    {
        REQUIRE(InRegistry.Register<TransformComponent>("Transform", bInTransformEssential));
        REQUIRE(InRegistry.Register<DummyComponent>("Dummy"));
        REQUIRE(InRegistry.Register<HealthComponent>("Health"));
    }

    /** Re-resolve a set of Guids to handles — the shape a caller has after a restore recreated one. */
    TDynArray<EntityID> HandlesOf(World& InWorld, const TDynArray<Guid>& InGuids)
    {
        TDynArray<EntityID> lHandles;

        for (const Guid& lGuid : InGuids)
        {
            Entity lEntity = InWorld.FindByGuid(lGuid);
            if (lEntity.IsValid()) { lHandles.emplace_back(lEntity.GetHandle()); }
        }

        return lHandles;
    }

    /** Drop one recorded component from an entity's payload — how a test states "the record predates it". */
    void EraseComponent(EntityData& InData, const OpaaxStringID InTypeName)
    {
        for (Uint64 lIndex = 0; lIndex < InData.Components.size(); ++lIndex)
        {
            if (InData.Components[lIndex].TypeName == InTypeName)
            {
                InData.Components.erase(InData.Components.begin() + static_cast<std::ptrdiff_t>(lIndex));
                return;
            }
        }
    }
}

// =============================================================================
// The gate
// =============================================================================
TEST_CASE("Restore: capture -> edit -> restore round-trips BYTE-FOR-BYTE")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMap = MapId("Level01");

    World lWorld("Restore");

    Entity lHero = lWorld.CreateEntity("Hero", lMap);
    lHero.Add<HealthComponent>(HealthComponent{100, 2.5f});
    lHero.Get<TransformComponent>().Position = Vector2F{12.f, -3.f};

    Entity lCrate = lWorld.CreateEntity("Crate", lMap);
    lCrate.Add<DummyComponent>();

    // An entity NOT in the record — it must come through the restore untouched.
    Entity lBystander = lWorld.CreateEntity("Bystander", lMap);
    lBystander.Get<TransformComponent>().Position = Vector2F{99.f, 99.f};

    const TDynArray<Guid>     lGuids{ lHero.GetGuid(), lCrate.GetGuid() };
    const TDynArray<EntityID> lTouched{ lHero.GetHandle(), lCrate.GetHandle() };

    const MapData     lBefore     = MapSerializer::CaptureEntities(lWorld, lRegistry, lTouched);
    const OpaaxString lBeforeText = MapJson::Serialize(lBefore);

    REQUIRE(lBefore.EntityCount() == 2u);

    // Every kind of edit the record has to survive, at once: a moved value, a renamed entity, a
    // component gained, a component lost, and one entity destroyed outright.
    lHero.Get<TransformComponent>().Position = Vector2F{-500.f, 7.f};
    lHero.Get<HealthComponent>().Hp          = 3;
    lHero.Get<EntityMeta>().Name             = "Renamed";
    lHero.Add<DummyComponent>();
    lCrate.Destroy();

    CHECK(MapFactory::Restore(lBefore, lWorld, lRegistry) == 2u);

    // Re-resolve by GUID, because the destroyed entity came back on a different handle.
    const MapData lAfter = MapSerializer::CaptureEntities(lWorld, lRegistry, HandlesOf(lWorld, lGuids));

    CHECK(lAfter.EntityCount() == 2u);
    CHECK(MapJson::Serialize(lAfter) == lBeforeText);

    // The entity the record never named kept its own edit-era state.
    REQUIRE(lBystander.IsValid());
    CHECK(lBystander.Get<TransformComponent>().Position.x == doctest::Approx(99.f));
}

// =============================================================================
// CaptureEntities — the subset question
// =============================================================================
TEST_CASE("CaptureEntities: takes ONLY what it was named, and leaves its Id invalid")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Subset");

    Entity lA = lWorld.CreateEntity("A", MapId("Level01"));
    lWorld.CreateEntity("B", MapId("Level01"));
    lWorld.CreateEntity("C", MapId("Level01"));

    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry, TDynArray<EntityID>{ lA.GetHandle() });

    REQUIRE(lData.EntityCount() == 1u);
    CHECK(lData.Entities[0].Name == "A");

    // A hand-picked set is not a map — the rule CaptureWorld already sets.
    CHECK_FALSE(lData.Id.IsValid());
}

TEST_CASE("CaptureEntities: a handle destroyed under the caller is SKIPPED, not an error")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Stale");

    Entity lKept  = lWorld.CreateEntity("Kept", MapId("Level01"));
    Entity lGone  = lWorld.CreateEntity("Gone", MapId("Level01"));

    const TDynArray<EntityID> lHandles{ lKept.GetHandle(), lGone.GetHandle() };
    lGone.Destroy();

    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry, lHandles);

    REQUIRE(lData.EntityCount() == 1u);
    CHECK(lData.Entities[0].Name == "Kept");
}

// =============================================================================
// Restore — the three jobs
// =============================================================================
TEST_CASE("Restore: recreates a DESTROYED entity under its original Guid")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Recreate");

    Entity lHero = lWorld.CreateEntity("Hero", MapId("Level01"));
    lHero.Add<HealthComponent>(HealthComponent{42, 1.f});

    const Guid    lGuid = lHero.GetGuid();
    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry,
                                                         TDynArray<EntityID>{ lHero.GetHandle() });

    lHero.Destroy();
    REQUIRE_FALSE(lWorld.FindByGuid(lGuid).IsValid());

    CHECK(MapFactory::Restore(lData, lWorld, lRegistry) == 1u);

    Entity lBack = lWorld.FindByGuid(lGuid);
    REQUIRE(lBack.IsValid());
    CHECK(lBack.Get<EntityMeta>().Name == "Hero");
    CHECK(lBack.Get<EntityMeta>().OwnerMap == MapId("Level01"));
    REQUIRE(lBack.Has<HealthComponent>());
    CHECK(lBack.Get<HealthComponent>() == HealthComponent{42, 1.f});
}

TEST_CASE("Restore: overwrites a LIVE entity — components and the name")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Overwrite");

    Entity lHero = lWorld.CreateEntity("Hero", MapId("Level01"));
    lHero.Add<HealthComponent>(HealthComponent{100, 2.f});

    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry,
                                                         TDynArray<EntityID>{ lHero.GetHandle() });

    lHero.Get<HealthComponent>().Hp = 1;
    lHero.Get<EntityMeta>().Name    = "Typo";

    CHECK(MapFactory::Restore(lData, lWorld, lRegistry) == 1u);

    // The SAME handle — a live entity is written through, never destroyed and rebuilt.
    CHECK(lHero.IsValid());
    CHECK(lHero.Get<HealthComponent>().Hp == 100);
    CHECK(lHero.Get<EntityMeta>().Name == "Hero");
}

TEST_CASE("Restore: REMOVES a component the record does not name — undo of an Add Component")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("RemoveUnrecorded");

    Entity lHero = lWorld.CreateEntity("Hero", MapId("Level01"));

    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry,
                                                         TDynArray<EntityID>{ lHero.GetHandle() });

    lHero.Add<DummyComponent>();
    REQUIRE(lHero.Has<DummyComponent>());

    CHECK(MapFactory::Restore(lData, lWorld, lRegistry) == 1u);
    CHECK_FALSE(lHero.Has<DummyComponent>());
}

TEST_CASE("Restore: an ESSENTIAL component the record lacks is refused and STAYS")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry, /*bInTransformEssential*/ true);

    World lWorld("Essential");

    Entity lHero = lWorld.CreateEntity("Hero", MapId("Level01"));

    MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry,
                                                   TDynArray<EntityID>{ lHero.GetHandle() });

    REQUIRE(lData.EntityCount() == 1u);

    // A record written before the type existed, or hand-edited: it does not name the transform.
    EraseComponent(lData.Entities[0], OpaaxStringID("Transform"));

    CHECK(MapFactory::Restore(lData, lWorld, lRegistry) == 1u);

    // I17 — every entity has one, so removing it must be impossible rather than merely discouraged.
    CHECK(lHero.Has<TransformComponent>());
}

TEST_CASE("Restore: bumps the world's revision, because an in-place write is invisible to it")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Revision");

    Entity lHero = lWorld.CreateEntity("Hero", MapId("Level01"));
    lHero.Add<HealthComponent>(HealthComponent{100, 2.f});

    const MapData lData = MapSerializer::CaptureEntities(lWorld, lRegistry,
                                                         TDynArray<EntityID>{ lHero.GetHandle() });

    lHero.Get<HealthComponent>().Hp = 1;

    const Uint64 lBefore = lWorld.GetRevision();

    CHECK(MapFactory::Restore(lData, lWorld, lRegistry) == 1u);
    CHECK(lWorld.GetRevision() > lBefore);
}

TEST_CASE("Restore: an EMPTY record touches nothing and does not move the revision")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Empty");

    lWorld.CreateEntity("Hero", MapId("Level01"));

    const Uint64 lBefore = lWorld.GetRevision();

    CHECK(MapFactory::Restore(MapData(), lWorld, lRegistry) == 0u);
    CHECK(lWorld.GetRevision() == lBefore);
    CHECK(lWorld.GetEntityCount() == 1u);
}
