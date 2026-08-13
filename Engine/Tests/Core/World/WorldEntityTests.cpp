// Suite: entity identity — Guid stamping, Guid-preserving creation, and the MapId partition.
//
// These two properties are what the M3 snapshot core rests on:
//   1. CreateEntityWithGuid can restore an identity that already exists, because capture ->
//      instantiate must preserve GUIDs or every inter-entity reference in a map breaks.
//   2. EntityMeta::OwnerMap says which Map authored an entity. A World owns ONE registry, so
//      a Map is a partition of it rather than a container; an invalid OwnerMap means the
//      entity was spawned at runtime and no map should ever write it out.
#include <doctest.h>

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

using namespace Opaax;

// =============================================================================
// Fresh creation
// =============================================================================
TEST_CASE("World: CreateEntity mints a valid Guid that resolves back to the same entity")
{
    World  lWorld("EntityTest");
    Entity lEntity = lWorld.CreateEntity("Hero");

    REQUIRE(lEntity.IsValid());

    const Guid lGuid = lEntity.GetGuid();
    CHECK(lGuid.IsValid());

    Entity lFound = lWorld.FindByGuid(lGuid);
    CHECK(lFound.IsValid());
    CHECK(lFound.GetHandle() == lEntity.GetHandle());
}

TEST_CASE("World: two entities never share a Guid")
{
    World lWorld("EntityTest");

    Entity lA = lWorld.CreateEntity("A");
    Entity lB = lWorld.CreateEntity("B");

    CHECK(lA.GetGuid() != lB.GetGuid());
}

// =============================================================================
// The MapId partition
// =============================================================================
TEST_CASE("World: an entity created without a map is runtime-spawned (invalid OwnerMap)")
{
    World  lWorld("EntityTest");
    Entity lEntity = lWorld.CreateEntity("Bullet");

    const EntityMeta& lMeta = lEntity.Get<EntityMeta>();

    // The default is deliberately "belongs to no map": a bullet spawned mid-play must not
    // end up serialized into the map file the player happens to be standing in.
    CHECK_FALSE(lMeta.OwnerMap.IsValid());
}

TEST_CASE("World: an entity created into a map carries that MapId")
{
    World lWorld("EntityTest");

    const MapId lMap = MapId("Level01");
    Entity      lEntity = lWorld.CreateEntity("Crate", lMap);

    const EntityMeta& lMeta = lEntity.Get<EntityMeta>();
    CHECK(lMeta.OwnerMap == lMap);
    CHECK(lMeta.OwnerMap.IsValid());
}

// =============================================================================
// Guid-preserving creation — the instantiate half
// =============================================================================
TEST_CASE("World: CreateEntityWithGuid restores the exact identity it was given")
{
    World lWorld("EntityTest");

    // A Guid minted elsewhere — stands in for one read back out of a captured map.
    const Guid  lWanted = Guid::New();
    const MapId lMap    = MapId("Level01");

    Entity lEntity = lWorld.CreateEntityWithGuid(lWanted, "Restored", lMap);

    REQUIRE(lEntity.IsValid());
    CHECK(lEntity.GetGuid() == lWanted);
    CHECK(lEntity.Get<EntityMeta>().Name == "Restored");
    CHECK(lEntity.Get<EntityMeta>().OwnerMap == lMap);

    // The GuidRegistry has to agree, or nothing can resolve a reference to this entity.
    CHECK(lWorld.FindByGuid(lWanted).GetHandle() == lEntity.GetHandle());
}

TEST_CASE("World: CreateEntityWithGuid refuses an invalid Guid")
{
    World lWorld("EntityTest");

    Entity lEntity = lWorld.CreateEntityWithGuid(Guid{}, "Nameless");

    CHECK_FALSE(lEntity.IsValid());
    CHECK(lWorld.GetEntityCount() == 0u); // refused means nothing was created
}

TEST_CASE("World: CreateEntityWithGuid refuses a Guid that is already live")
{
    World lWorld("EntityTest");

    Entity     lFirst = lWorld.CreateEntity("Original");
    const Guid lGuid  = lFirst.GetGuid();

    Entity lDuplicate = lWorld.CreateEntityWithGuid(lGuid, "Impostor");

    CHECK_FALSE(lDuplicate.IsValid());
    CHECK(lWorld.GetEntityCount() == 1u);

    // The original mapping must be untouched — the danger of allowing this is that the
    // second Register silently evicts the first and FindByGuid starts answering the impostor.
    Entity lFound = lWorld.FindByGuid(lGuid);
    CHECK(lFound.GetHandle() == lFirst.GetHandle());
    CHECK(lFound.Get<EntityMeta>().Name == "Original");
}

TEST_CASE("World: a destroyed entity releases its Guid for re-creation")
{
    World lWorld("EntityTest");

    Entity     lEntity = lWorld.CreateEntity("Temporary");
    const Guid lGuid   = lEntity.GetGuid();

    lEntity.Destroy();
    CHECK_FALSE(lWorld.FindByGuid(lGuid).IsValid());

    // This is the capture -> Clear -> instantiate path in miniature: the same Guid must be
    // re-creatable once the old holder is gone, or a round trip into a reused world fails.
    Entity lRestored = lWorld.CreateEntityWithGuid(lGuid, "Restored");
    REQUIRE(lRestored.IsValid());
    CHECK(lRestored.GetGuid() == lGuid);
}

TEST_CASE("World: Clear releases every Guid")
{
    World lWorld("EntityTest");

    const Guid lGuid = lWorld.CreateEntity("Doomed").GetGuid();
    lWorld.CreateEntity("AlsoDoomed");

    lWorld.Clear();
    CHECK(lWorld.GetEntityCount() == 0u);

    // Instantiate-into-a-cleared-world is exactly what the round-trip gate does.
    CHECK(lWorld.CreateEntityWithGuid(lGuid, "Restored").IsValid());
}

// =============================================================================
// Revision — the editor's dirty-check gate
// =============================================================================
//
// The editor skips a whole capture + serialize per mounted map when this has not moved
// (EditorLevelDocument::RefreshDirty, MP5). A mutation that fails to bump it therefore reads as
// "no unsaved changes" — silently, and until something unrelated bumps it. These pin the
// chokepoints the gate is allowed to trust.
TEST_CASE("World: the revision moves on every entity create and destroy")
{
    World lWorld("RevisionTest");

    const Uint64 lStart = lWorld.GetRevision();

    Entity lEntity = lWorld.CreateEntity("Hero");
    const Uint64 lAfterCreate = lWorld.GetRevision();
    CHECK(lAfterCreate != lStart);

    lEntity.Destroy();
    CHECK(lWorld.GetRevision() != lAfterCreate);
}

TEST_CASE("World: the revision moves on CreateEntityWithGuid — the instantiate path")
{
    World lWorld("RevisionTest");

    // Mounting a map goes through here, not through CreateEntity. A gate that missed it would
    // leave a freshly mounted map reading clean against a baseline it never took.
    const Uint64 lBefore = lWorld.GetRevision();

    REQUIRE(lWorld.CreateEntityWithGuid(Guid::New(), "Restored").IsValid());
    CHECK(lWorld.GetRevision() != lBefore);
}

TEST_CASE("World: a REFUSED create does not move the revision")
{
    World lWorld("RevisionTest");

    const Guid lGuid = lWorld.CreateEntity("Original").GetGuid();
    const Uint64 lBefore = lWorld.GetRevision();

    // A duplicate Guid is refused (WM3) — nothing was added, so nothing changed.
    CHECK_FALSE(lWorld.CreateEntityWithGuid(lGuid, "Duplicate").IsValid());
    CHECK(lWorld.GetRevision() == lBefore);
}

TEST_CASE("World: Clear moves the revision")
{
    World lWorld("RevisionTest");
    lWorld.CreateEntity("Doomed");

    const Uint64 lBefore = lWorld.GetRevision();

    lWorld.Clear();
    CHECK(lWorld.GetRevision() != lBefore);
}

TEST_CASE("World: MarkChanged moves the revision — the in-place component edit")
{
    World lWorld("RevisionTest");

    // The Inspector's drawers write straight through a TComponent&, so no World method and no
    // entt signal sees a field edit. This is the only way that mutation reaches the gate.
    const Uint64 lBefore = lWorld.GetRevision();

    lWorld.MarkChanged();
    CHECK(lWorld.GetRevision() != lBefore);
}

TEST_CASE("World: the revision is MONOTONIC — it never returns to an earlier value")
{
    World lWorld("RevisionTest");

    // The gate stores the last value it checked at and compares. A revision that could go
    // BACKWARDS could land on that stored value and make a changed world read as unchanged.
    Uint64 lPrevious = lWorld.GetRevision();

    for (int lStep = 0; lStep < 8; ++lStep)
    {
        Entity lEntity = lWorld.CreateEntity("Churn");
        CHECK(lWorld.GetRevision() > lPrevious);
        lPrevious = lWorld.GetRevision();

        lEntity.Destroy();
        CHECK(lWorld.GetRevision() > lPrevious);
        lPrevious = lWorld.GetRevision();
    }

    lWorld.Clear();
    CHECK(lWorld.GetRevision() > lPrevious);
}
