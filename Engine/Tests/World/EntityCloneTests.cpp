// Suite: EntityClone — copies of captured entities with new identities (the editor's copy, paste and
// duplicate). Checked: new guids, links kept inside the set, outside parents kept or cleared, a whole
// prefab instance still a placement (it folds), a partial one unlinked, the source untouched.
#include <doctest.h>

#include "Core/OpaaxTypes.h"

#include "Renderer/Components/QuadComponent.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Serialization/EntityClone.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<TransformComponent>("Transform", /*bEssential*/true));
        REQUIRE(InRegistry.Register<QuadComponent>("Quad"));
        REQUIRE(InRegistry.Register<PrefabInstanceComponent>("PrefabInstance"));
    }

    const EntityData* FindByName(const MapData& InData, const char* InName)
    {
        for (const EntityData& lEntity : InData.Entities)
        {
            if (lEntity.Name == OpaaxString(InName)) { return &lEntity; }
        }
        return nullptr;
    }

    /** A two-entity prefab (Body, Muzzle under it), without map. */
    PrefabData MakePrefab(const ComponentRegistry& InRegistry)
    {
        World lAuthoring("Authoring");

        Entity lBody = lAuthoring.CreateEntity("Body", MapId("Source"));
        lBody.Add<QuadComponent>();
        Entity lMuzzle = lAuthoring.CreateEntity("Muzzle", MapId("Source"));
        EntityHierarchy::SetParent(lMuzzle, lBody);

        PrefabData lPrefab;
        lPrefab.Entities = MapSerializer::CaptureMap(lAuthoring, InRegistry, MapId("Source")).Entities;
        for (EntityData& lEntity : lPrefab.Entities) { lEntity.OwnerMap = MapId(); }
        return lPrefab;
    }

    class OnePrefab final : public IPrefabResolver
    {
    public:
        OnePrefab(const char* InPath, const PrefabData& InData) : m_Path(InPath), m_Data(InData) {}

        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override
        {
            return InAssetPath == m_Path ? &m_Data : nullptr;
        }

    private:
        OpaaxString m_Path;
        PrefabData  m_Data;
    };
}

TEST_CASE("EntityClone: every copy has a new guid, and a parent in the set names its copy")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World  lWorld("Source");
    Entity lParent = lWorld.CreateEntity("Parent", MapId("Level"));
    Entity lChild  = lWorld.CreateEntity("Child", MapId("Level"));
    lChild.Add<QuadComponent>();
    EntityHierarchy::SetParent(lChild, lParent);

    const MapData lSource = MapSerializer::CaptureMap(lWorld, lRegistry, MapId("Level"));
    const MapData lCopy   = EntityClone::Clone(lSource, {}, lRegistry);

    REQUIRE(lCopy.EntityCount() == 2);

    const EntityData* lParentCopy = FindByName(lCopy, "Parent");
    const EntityData* lChildCopy  = FindByName(lCopy, "Child");
    REQUIRE(lParentCopy != nullptr);
    REQUIRE(lChildCopy != nullptr);

    CHECK(lParentCopy->Id != lParent.GetGuid());
    CHECK(lChildCopy->Id != lChild.GetGuid());
    CHECK(lChildCopy->Parent == lParentCopy->Id);
    CHECK(lChildCopy->OwnerMap == MapId("Level"));
    CHECK(lChildCopy->Components.size() == FindByName(lSource, "Child")->Components.size());

    // The source is untouched, and the copies go in beside the originals.
    CHECK(FindByName(lSource, "Child")->Id == lChild.GetGuid());
    CHECK(MapFactory::Instantiate(lCopy, lWorld, lRegistry) == 2);
    CHECK(lWorld.GetEntityCount() == 4);
}

TEST_CASE("EntityClone: a parent outside the set is kept for a duplicate, cleared for a paste")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World  lWorld("Source");
    Entity lParent = lWorld.CreateEntity("Parent", MapId("Level"));
    Entity lChild  = lWorld.CreateEntity("Child", MapId("Level"));
    EntityHierarchy::SetParent(lChild, lParent);

    const MapData lSource = MapSerializer::CaptureEntities(lWorld, lRegistry, { lChild.GetHandle() });

    EntityClone::Options lDuplicate;
    lDuplicate.bKeepOutsideParents = true;
    CHECK(EntityClone::Clone(lSource, lDuplicate, lRegistry).Entities.front().Parent == lParent.GetGuid());

    CHECK_FALSE(EntityClone::Clone(lSource, {}, lRegistry).Entities.front().Parent.IsValid());
}

TEST_CASE("EntityClone: the owner map is the one asked for, or each entity's own")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Source");
    lWorld.CreateEntity("A", MapId("First"));
    const MapData lSource = MapSerializer::CaptureMap(lWorld, lRegistry, MapId("First"));

    EntityClone::Options lInto;
    lInto.OwnerMap = MapId("Second");

    CHECK(EntityClone::Clone(lSource, lInto, lRegistry).Entities.front().OwnerMap == MapId("Second"));
    CHECK(EntityClone::Clone(lSource, {}, lRegistry).Entities.front().OwnerMap == MapId("First"));
}

TEST_CASE("EntityClone: a whole prefab instance becomes a placement of its own, which still folds")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData  lPrefab = MakePrefab(lRegistry);
    const char*       lPath   = "Prefabs/Gun.opaaxprefab";
    const Guid        lFirst  = Guid::New();
    const MapData     lPlaced = PrefabFactory::BuildInstance(lPrefab, lPath, lFirst, MapId("Level"), lRegistry);

    REQUIRE(EntityClone::InstancesOf(lPlaced, lRegistry).size() == 1);
    CHECK(EntityClone::CountOfInstance(lPlaced, lFirst, lRegistry) == 2);

    const MapData lCopy = EntityClone::Clone(lPlaced, {}, lRegistry);

    const TDynArray<Guid> lInstances = EntityClone::InstancesOf(lCopy, lRegistry);
    REQUIRE(lInstances.size() == 1);
    CHECK(lInstances.front() != lFirst);

    // Named as BuildInstance names them, so the map folds the copy as it does a placement.
    for (const EntityData& lEntity : lCopy.Entities)
    {
        bool bDerived = false;
        for (const EntityData& lTemplate : lPrefab.Entities)
        {
            bDerived = bDerived || lEntity.Id == Guid::Derive(lInstances.front(), lTemplate.Id);
        }
        CHECK(bDerived);
    }

    World lWorld("Target");
    CHECK(MapFactory::Instantiate(lPlaced, lWorld, lRegistry) == 2);
    CHECK(MapFactory::Instantiate(lCopy, lWorld, lRegistry) == 2);

    MapData lSaved = MapSerializer::CaptureMap(lWorld, lRegistry, MapId("Level"));
    CHECK(PrefabFold::Fold(lSaved, OnePrefab(lPath, lPrefab), lRegistry) == 2);
    CHECK(lSaved.Entities.empty());
}

TEST_CASE("EntityClone: part of an instance, unlinked, is plain entities")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    const Guid       lId     = Guid::New();
    MapData lPart = PrefabFactory::BuildInstance(lPrefab, "Prefabs/Gun.opaaxprefab", lId, MapId("Level"), lRegistry);
    lPart.Entities.pop_back();

    CHECK(EntityClone::Unlink(lPart, lId, lRegistry) == 1);
    CHECK(EntityClone::InstancesOf(lPart, lRegistry).empty());

    // A plain copy: a random guid, not a derived one.
    const MapData lCopy = EntityClone::Clone(lPart, {}, lRegistry);
    REQUIRE(lCopy.EntityCount() == 1);
    CHECK(lCopy.Entities.front().Id != lPart.Entities.front().Id);
    CHECK(EntityClone::InstancesOf(lCopy, lRegistry).empty());
}

TEST_CASE("EntityClone: two copies of one source never share a guid")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    const MapData    lPlaced = PrefabFactory::BuildInstance(lPrefab, "Prefabs/Gun.opaaxprefab", Guid::New(),
                                                            MapId("Level"), lRegistry);

    const MapData lOne = EntityClone::Clone(lPlaced, {}, lRegistry);
    const MapData lTwo = EntityClone::Clone(lPlaced, {}, lRegistry);

    for (const EntityData& lLeft : lOne.Entities)
    {
        for (const EntityData& lRight : lTwo.Entities)
        {
            CHECK(lLeft.Id != lRight.Id);
        }
    }
}
