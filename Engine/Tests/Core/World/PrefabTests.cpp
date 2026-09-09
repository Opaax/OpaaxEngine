// Suite: ⑦-C P1 — the prefab as a document (PrefabJson / PrefabFile / PrefabResource) and as a
// placement (PrefabFactory::BuildInstance -> MapFactory::Instantiate).
//
// THE GATE IS "TWICE INTO ONE WORLD". A prefab file carries the guids it authored, and
// World::CreateEntityWithGuid REFUSES one already live (WM3) — so before Guid::Derive existed, a
// second instance of one prefab was not merely wrong, it silently did not appear. Every other case
// here is scaffolding around that one.
//
// Runs against a UNIQUE directory under the OS temp dir, created and removed per case — the suite
// never touches the repo, and never the editor's own files ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <string>
#include <unordered_set>

#include "Engine/Subsystems/Resources/ResourceManager.h"   // before PrefabResource — completes LoadContext
#include "World/Components/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabJson.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // One temp directory per case, removed on scope exit — MapFileTests' shape.
    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxPrefabTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);          // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        OpaaxString Sub(const char* InRel) const
        {
            return OpaaxString((m_Path / InRel).generic_string().c_str());
        }

    private:
        fs::path m_Path;
    };

    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<TransformComponent>("Transform", /*bEssential*/true));
        REQUIRE(InRegistry.Register<DummyComponent>("Dummy"));
        REQUIRE(InRegistry.Register<PrefabInstanceComponent>("PrefabInstance"));
    }

    // A two-entity prefab, authored the way one is CREATED: by capturing entities out of a world
    // and dropping their map membership. That is P2's verb; doing it by hand here keeps this suite
    // independent of the editor.
    PrefabData MakePrefab(const ComponentRegistry& InRegistry)
    {
        World lAuthoring("Authoring");

        Entity lBody = lAuthoring.CreateEntity("Body", MapId("Source"));
        lBody.Get<TransformComponent>().Position = Vector2F{3.f, 4.f};
        lBody.Add<DummyComponent>();

        Entity lMuzzle = lAuthoring.CreateEntity("Muzzle", MapId("Source"));
        lMuzzle.Get<TransformComponent>().Position = Vector2F{-1.f, 0.5f};

        MapData lCaptured = MapSerializer::CaptureMap(lAuthoring, InRegistry, MapId("Source"));

        PrefabData lPrefab;
        lPrefab.Entities = Move(lCaptured.Entities);

        // A prefab's entities belong to no map (**WM2**) — an instance stamps one.
        for (EntityData& lEntity : lPrefab.Entities)
        {
            lEntity.OwnerMap = MapId();
        }

        return lPrefab;
    }

    const PrefabInstanceComponent* FindMarker(Entity InEntity)
    {
        return InEntity.Has<PrefabInstanceComponent>() ? &InEntity.Get<PrefabInstanceComponent>() : nullptr;
    }
}

// =============================================================================
// THE GATE
// =============================================================================
TEST_CASE("Prefab: one prefab instantiated TWICE into one world gives two live instances")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    REQUIRE(lPrefab.EntityCount() == 2);

    const MapId       lMap  = MapId("Level01");
    const OpaaxString lPath = OpaaxString("Prefabs/Gun.opaaxprefab");

    World lWorld("Target");

    const Guid lFirstId  = Guid::New();
    const Guid lSecondId = Guid::New();

    MapData lFirst  = PrefabFactory::BuildInstance(lPrefab, lPath, lFirstId,  lMap, lRegistry);
    MapData lSecond = PrefabFactory::BuildInstance(lPrefab, lPath, lSecondId, lMap, lRegistry);

    REQUIRE(lFirst.EntityCount()  == 2);
    REQUIRE(lSecond.EntityCount() == 2);

    // Both go in through the UNCHANGED MapFactory::Instantiate — the whole point of doing the
    // remap as a MapData -> MapData pass in front of it (**K2**).
    CHECK(MapFactory::Instantiate(lFirst,  lWorld, lRegistry) == 2);
    CHECK(MapFactory::Instantiate(lSecond, lWorld, lRegistry) == 2);

    // FOUR entities, not two. A count of 2 is precisely what the old behaviour would have
    // produced — Instantiate refusing every guid of the second instance, loudly but harmlessly
    // enough that a screenshot would look almost right.
    Uint64 lCount = 0;
    lWorld.Each<EntityMeta>([&lCount](const EntityMeta&) { ++lCount; });
    CHECK(lCount == 4);

    // Asked a second way, of the world's own counter rather than of a view over its registry —
    // two instruments, because a registry walk and a tracked count can disagree.
    CHECK(lWorld.GetEntityCount() == 4);

    // Every identity distinct.
    std::unordered_set<Guid> lIds;
    for (const EntityData& lEntity : lFirst.Entities)  { lIds.insert(lEntity.Id); }
    for (const EntityData& lEntity : lSecond.Entities) { lIds.insert(lEntity.Id); }
    CHECK(lIds.size() == 4);

    // And each is actually resolvable in the world, which the set above does not say.
    for (const Guid& lId : lIds)
    {
        CHECK(lWorld.FindByGuid(lId).IsValid());
    }
}

TEST_CASE("Prefab: the two instances are TOLD APART by InstanceId, not by the prefab path")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData  lPrefab = MakePrefab(lRegistry);
    const MapId       lMap    = MapId("Level01");
    const OpaaxString lPath   = OpaaxString("Prefabs/Gun.opaaxprefab");

    World lWorld("Target");

    const Guid lFirstId  = Guid::New();
    const Guid lSecondId = Guid::New();

    MapData lFirst  = PrefabFactory::BuildInstance(lPrefab, lPath, lFirstId,  lMap, lRegistry);
    MapData lSecond = PrefabFactory::BuildInstance(lPrefab, lPath, lSecondId, lMap, lRegistry);

    const Guid lFirstBodyId  = lFirst.Entities[0].Id;
    const Guid lSecondBodyId = lSecond.Entities[0].Id;

    REQUIRE(MapFactory::Instantiate(lFirst,  lWorld, lRegistry) == 2);
    REQUIRE(MapFactory::Instantiate(lSecond, lWorld, lRegistry) == 2);

    const PrefabInstanceComponent* lFirstMarker  = FindMarker(lWorld.FindByGuid(lFirstBodyId));
    const PrefabInstanceComponent* lSecondMarker = FindMarker(lWorld.FindByGuid(lSecondBodyId));

    REQUIRE(lFirstMarker  != nullptr);
    REQUIRE(lSecondMarker != nullptr);

    // Same prefab...
    CHECK(lFirstMarker->Prefab.Path == lPath);
    CHECK(lSecondMarker->Prefab.Path == lPath);

    // ...different placement, and each entity still knows which entity OF the prefab it is.
    CHECK(lFirstMarker->InstanceId == lFirstId);
    CHECK(lSecondMarker->InstanceId == lSecondId);
    CHECK(lFirstMarker->TemplateGuid == lSecondMarker->TemplateGuid);
    CHECK(lFirstMarker->IsLinked());
}

TEST_CASE("Prefab: BuildInstance is REPEATABLE — the same instance id lands on the same entities")
{
    // What makes re-applying a changed prefab (⑦-C P4) safe: it must find the entities it already
    // created rather than making a second set beside them.
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData  lPrefab   = MakePrefab(lRegistry);
    const OpaaxString lPath     = OpaaxString("Prefabs/Gun.opaaxprefab");
    const Guid        lInstance = Guid::New();

    const MapData lOnce  = PrefabFactory::BuildInstance(lPrefab, lPath, lInstance, MapId("L"), lRegistry);
    const MapData lTwice = PrefabFactory::BuildInstance(lPrefab, lPath, lInstance, MapId("L"), lRegistry);

    REQUIRE(lOnce.EntityCount() == lTwice.EntityCount());
    for (Uint64 lIndex = 0; lIndex < lOnce.EntityCount(); ++lIndex)
    {
        CHECK(lOnce.Entities[lIndex].Id == lTwice.Entities[lIndex].Id);
    }
}

// =============================================================================
// BuildInstance's stamping and its refusals
// =============================================================================
TEST_CASE("Prefab: an instance's entities are stamped into the TARGET map")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    const MapId      lMap    = MapId("Level01");

    const MapData lInstance = PrefabFactory::BuildInstance(
        lPrefab, OpaaxString("Prefabs/Gun.opaaxprefab"), Guid::New(), lMap, lRegistry);

    CHECK(lInstance.Id == lMap);
    for (const EntityData& lEntity : lInstance.Entities)
    {
        // An invalid OwnerMap reads as runtime-spawned (**WM2**), and no Save would ever write it —
        // a placement that silently does not persist.
        CHECK(lEntity.OwnerMap == lMap);
    }
}

TEST_CASE("Prefab: BuildInstance REFUSES rather than building something untraceable")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData  lPrefab = MakePrefab(lRegistry);
    const OpaaxString lPath   = OpaaxString("Prefabs/Gun.opaaxprefab");

    // No instance id: every entity would derive from the invalid guid, so two instances would
    // collide again — the exact bug this all exists to prevent.
    CHECK(PrefabFactory::BuildInstance(lPrefab, lPath, Guid{}, MapId("L"), lRegistry).IsEmpty());

    // No target map: see the stamping case above.
    CHECK(PrefabFactory::BuildInstance(lPrefab, lPath, Guid::New(), MapId(), lRegistry).IsEmpty());

    // No marker type registered: the instance would exist and nothing could ever find it again.
    ComponentRegistry lNoMarker;
    REQUIRE(lNoMarker.Register<TransformComponent>("Transform", /*bEssential*/true));
    CHECK(PrefabFactory::BuildInstance(lPrefab, lPath, Guid::New(), MapId("L"), lNoMarker).IsEmpty());
}

TEST_CASE("Prefab: a marker already in the FILE is replaced, never doubled")
{
    // A prefab authored from entities that were themselves an instance carries a stale marker.
    // Composing the two is nesting (⑦-C P7); until then the outer instance owns the entity, and
    // what must never happen is the payload appearing twice on one entity.
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    PrefabData lPrefab = MakePrefab(lRegistry);

    PrefabInstanceComponent lStale;
    lStale.Prefab.Path  = OpaaxString("Prefabs/Old.opaaxprefab");
    lStale.InstanceId   = Guid::New();
    lStale.TemplateGuid = Guid::New();
    lPrefab.Entities[0].Components.emplace_back(OpaaxStringID("PrefabInstance"), nlohmann::json(lStale));

    const OpaaxString lPath = OpaaxString("Prefabs/Gun.opaaxprefab");
    const MapData lInstance = PrefabFactory::BuildInstance(lPrefab, lPath, Guid::New(), MapId("L"), lRegistry);

    Uint64 lMarkers = 0;
    for (const ComponentData& lComponent : lInstance.Entities[0].Components)
    {
        if (lComponent.TypeName == OpaaxStringID("PrefabInstance")) { ++lMarkers; }
    }

    CHECK(lMarkers == 1);
    CHECK(lInstance.Entities[0].Components.back().Payload.at("Prefab").get<std::string>()
          == std::string("Prefabs/Gun.opaaxprefab"));
}

// =============================================================================
// BuildPrefab — the other direction (⑦-C P2)
// =============================================================================
TEST_CASE("Prefab: BuildPrefab clears OwnerMap and KEEPS the guids")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Authoring");
    Entity lA = lWorld.CreateEntity("A", MapId("Level01"));
    Entity lB = lWorld.CreateEntity("B", MapId("Level01"));

    const MapData lCaptured = MapSerializer::CaptureEntities(
        lWorld, lRegistry, { lA.GetHandle(), lB.GetHandle() });

    const PrefabData lPrefab = PrefabFactory::BuildPrefab(lCaptured, lRegistry);

    REQUIRE(lPrefab.EntityCount() == 2);
    for (const EntityData& lEntity : lPrefab.Entities)
    {
        // Belongs to no map — an instance is what stamps one (**WM2**).
        CHECK_FALSE(lEntity.OwnerMap.IsValid());
    }

    // The guids become the file's TEMPLATE ids, so they must survive: BuildInstance derives from
    // them and an override record will key by them.
    std::unordered_set<Guid> lKept;
    for (const EntityData& lEntity : lPrefab.Entities) { lKept.insert(lEntity.Id); }
    CHECK(lKept.count(lA.GetGuid()) == 1);
    CHECK(lKept.count(lB.GetGuid()) == 1);
}

TEST_CASE("Prefab: BuildPrefab STRIPS an existing instance marker rather than baking it in")
{
    // Making a prefab out of entities that were themselves an instance must not produce a file
    // whose entities claim to belong to a DIFFERENT prefab. Nesting is P7; flattening is the
    // honest answer until then.
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lSource = MakePrefab(lRegistry);
    const MapId      lMap    = MapId("Level01");

    World   lWorld("Target");
    MapData lInstance = PrefabFactory::BuildInstance(
        lSource, OpaaxString("Prefabs/Gun.opaaxprefab"), Guid::New(), lMap, lRegistry);
    REQUIRE(MapFactory::Instantiate(lInstance, lWorld, lRegistry) == 2);

    // Capture what the instance produced — entities that DO carry a marker.
    TDynArray<EntityID> lHandles;
    for (const EntityData& lEntity : lInstance.Entities)
    {
        lHandles.emplace_back(lWorld.FindByGuid(lEntity.Id).GetHandle());
    }

    const MapData    lCaptured = MapSerializer::CaptureEntities(lWorld, lRegistry, lHandles);
    const PrefabData lRebuilt  = PrefabFactory::BuildPrefab(lCaptured, lRegistry);

    REQUIRE(lRebuilt.EntityCount() == 2);
    for (const EntityData& lEntity : lRebuilt.Entities)
    {
        for (const ComponentData& lComponent : lEntity.Components)
        {
            CHECK(lComponent.TypeName != OpaaxStringID("PrefabInstance"));
        }
    }
}

TEST_CASE("Prefab: the FULL round trip — world entities -> prefab -> file -> two placements")
{
    // What "Create Prefab from Selection" is, end to end and headless. The two placements at the
    // end are what make it a prefab rather than an export.
    const ScopedTempDir lTemp("full_round_trip");
    const OpaaxString   lPath = lTemp.Sub("Made.opaaxprefab");

    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World lWorld("Session");
    Entity lA = lWorld.CreateEntity("Part A", MapId("Level01"));
    lA.Get<TransformComponent>().Position = Vector2F{7.f, 8.f};
    lA.Add<DummyComponent>();
    Entity lB = lWorld.CreateEntity("Part B", MapId("Level01"));

    const MapData    lCaptured = MapSerializer::CaptureEntities(
        lWorld, lRegistry, { lA.GetHandle(), lB.GetHandle() });
    const PrefabData lPrefab   = PrefabFactory::BuildPrefab(lCaptured, lRegistry);

    REQUIRE(PrefabFile::Save(lPath, lPrefab));

    PrefabData lReloaded;
    REQUIRE(PrefabFile::Load(lPath, lReloaded));
    REQUIRE(lReloaded.EntityCount() == 2);

    MapData lFirst = PrefabFactory::BuildInstance(lReloaded, OpaaxString("P/Made.opaaxprefab"),
                                                  Guid::New(), MapId("Level01"), lRegistry);
    MapData lSecond = PrefabFactory::BuildInstance(lReloaded, OpaaxString("P/Made.opaaxprefab"),
                                                   Guid::New(), MapId("Level01"), lRegistry);

    CHECK(MapFactory::Instantiate(lFirst,  lWorld, lRegistry) == 2);
    CHECK(MapFactory::Instantiate(lSecond, lWorld, lRegistry) == 2);

    // The two originals plus two placements of two.
    CHECK(lWorld.GetEntityCount() == 6);

    // The authored data survived the whole trip, not merely the entity count.
    Entity lPlaced = lWorld.FindByGuid(lFirst.Entities[0].Id);
    REQUIRE(lPlaced.IsValid());
    const bool lIsPartA = lPlaced.Get<EntityMeta>().Name == OpaaxString("Part A");
    if (lIsPartA)
    {
        CHECK(lPlaced.Get<TransformComponent>().Position.x == doctest::Approx(7.f));
        CHECK(lPlaced.Has<DummyComponent>());
    }
}

// =============================================================================
// PrefabFold — entities <-> instance records (⑦-C P3)
// =============================================================================
namespace
{
    // A resolver backed by a plain table; the editor's is backed by the ResourceManager.
    class StubResolver final : public IPrefabResolver
    {
    public:
        void Add(const char* InPath, PrefabData InData)
        {
            m_Paths.emplace_back(OpaaxString(InPath));
            m_Data.emplace_back(Move(InData));
        }

        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override
        {
            for (Uint64 lIndex = 0; lIndex < m_Paths.size(); ++lIndex)
            {
                if (m_Paths[lIndex] == InAssetPath) { return &m_Data[lIndex]; }
            }
            return nullptr;
        }

    private:
        TDynArray<OpaaxString> m_Paths;
        TDynArray<PrefabData>  m_Data;
    };

    // A world holding one placement of InPrefab, captured as a MapData ready to fold.
    MapData PlaceAndCapture(World& InWorld, const PrefabData& InPrefab, const ComponentRegistry& InRegistry,
                            const char* InPath, const Guid& InInstanceId, MapId InMap)
    {
        MapData lInstance = PrefabFactory::BuildInstance(InPrefab, OpaaxString(InPath), InInstanceId,
                                                         InMap, InRegistry);
        REQUIRE(MapFactory::Instantiate(lInstance, InWorld, InRegistry) == InPrefab.EntityCount());

        return MapSerializer::CaptureMap(InWorld, InRegistry, InMap);
    }
}

TEST_CASE("PrefabFold: two placements fold to two RECORDS and no entities")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    StubResolver     lResolver;
    lResolver.Add("Prefabs/Gun.opaaxprefab", lPrefab);

    const MapId lMap = MapId("Level01");
    World       lWorld("W");

    PlaceAndCapture(lWorld, lPrefab, lRegistry, "Prefabs/Gun.opaaxprefab", Guid::New(), lMap);
    MapData lCaptured = PlaceAndCapture(lWorld, lPrefab, lRegistry, "Prefabs/Gun.opaaxprefab",
                                        Guid::New(), lMap);

    REQUIRE(lCaptured.EntityCount() == 4);

    CHECK(PrefabFold::Fold(lCaptured, lResolver, lRegistry) == 2);
    CHECK(lCaptured.EntityCount() == 0);        // all four folded away
    CHECK(lCaptured.InstanceCount() == 2);

    // An untouched placement carries NO overrides — the common case, and what keeps a map small.
    for (const PrefabInstanceRecord& lRecord : lCaptured.Instances)
    {
        CHECK(lRecord.Overrides.empty());
    }
}

TEST_CASE("PrefabFold: Fold -> Expand reproduces the entities, guids included")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    StubResolver     lResolver;
    lResolver.Add("Prefabs/Gun.opaaxprefab", lPrefab);

    const MapId lMap = MapId("Level01");
    World       lWorld("W");

    MapData lCaptured = PlaceAndCapture(lWorld, lPrefab, lRegistry, "Prefabs/Gun.opaaxprefab",
                                        Guid::New(), lMap);

    std::unordered_set<Guid> lBefore;
    for (const EntityData& lEntity : lCaptured.Entities) { lBefore.insert(lEntity.Id); }

    REQUIRE(PrefabFold::Fold(lCaptured, lResolver, lRegistry) == 1);
    REQUIRE(PrefabFold::Expand(lCaptured, lResolver, lRegistry) == 1);

    REQUIRE(lCaptured.EntityCount() == 2);
    CHECK(lCaptured.InstanceCount() == 0);

    // The guids came back IDENTICAL, which is what an inter-entity reference survives on (**WM3**)
    // — and it works because Expand re-derives rather than re-mints.
    std::unordered_set<Guid> lAfter;
    for (const EntityData& lEntity : lCaptured.Entities) { lAfter.insert(lEntity.Id); }
    CHECK(lAfter == lBefore);
}

TEST_CASE("PrefabFold: THE GATE — an override survives, an untouched property follows the prefab")
{
    // The whole point of storing a patch rather than the entities. Place, override ONE property,
    // then change the prefab in a DIFFERENT property, and both must hold.
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    PrefabData lPrefab = MakePrefab(lRegistry);
    const MapId lMap   = MapId("Level01");

    StubResolver lResolver;
    lResolver.Add("Prefabs/Gun.opaaxprefab", lPrefab);

    World         lWorld("W");
    const Guid    lInstanceId = Guid::New();
    MapData lInstance = PrefabFactory::BuildInstance(lPrefab, OpaaxString("Prefabs/Gun.opaaxprefab"),
                                                     lInstanceId, lMap, lRegistry);
    REQUIRE(MapFactory::Instantiate(lInstance, lWorld, lRegistry) == 2);

    // The author moves one entity — an override on Transform.Position.
    Entity lPlaced = lWorld.FindByGuid(lInstance.Entities[0].Id);
    REQUIRE(lPlaced.IsValid());
    lPlaced.Get<TransformComponent>().Position = Vector2F{999.f, 111.f};

    MapData lCaptured = MapSerializer::CaptureMap(lWorld, lRegistry, lMap);
    REQUIRE(PrefabFold::Fold(lCaptured, lResolver, lRegistry) == 1);
    REQUIRE(lCaptured.Instances[0].Overrides.size() == 1);   // ONLY the moved entity

    // Now the PREFAB changes a property nobody overrode: the same entity's Rotation.
    for (EntityData& lTemplate : lPrefab.Entities)
    {
        if (lTemplate.Id != lCaptured.Instances[0].Overrides[0].TemplateGuid) { continue; }

        for (ComponentData& lComponent : lTemplate.Components)
        {
            if (lComponent.TypeName == OpaaxStringID("Transform")) { lComponent.Payload["Rotation"] = 45.0; }
        }
    }

    StubResolver lNewResolver;
    lNewResolver.Add("Prefabs/Gun.opaaxprefab", lPrefab);

    REQUIRE(PrefabFold::Expand(lCaptured, lNewResolver, lRegistry) == 1);

    const EntityData* lResult = nullptr;
    for (const EntityData& lEntity : lCaptured.Entities)
    {
        if (lEntity.Id == lPlaced.GetGuid()) { lResult = &lEntity; }
    }
    REQUIRE(lResult != nullptr);

    for (const ComponentData& lComponent : lResult->Components)
    {
        if (lComponent.TypeName != OpaaxStringID("Transform")) { continue; }

        // The override HELD...
        CHECK(lComponent.Payload["Position"]["x"].get<float>() == doctest::Approx(999.f));
        // ...and the prefab's change ARRIVED. Storing the entities instead of a patch would
        // report 0 here, and no later prefab edit would ever reach this map again.
        CHECK(lComponent.Payload["Rotation"].get<float>() == doctest::Approx(45.f));
    }
}

TEST_CASE("PrefabFold: an UNRESOLVABLE prefab keeps its entities rather than losing them")
{
    // A renamed or deleted prefab file must cost the author a link, never their level.
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    const MapId      lMap    = MapId("Level01");

    World   lWorld("W");
    MapData lCaptured = PlaceAndCapture(lWorld, lPrefab, lRegistry, "Prefabs/Gone.opaaxprefab",
                                        Guid::New(), lMap);

    StubResolver lEmpty;   // resolves nothing

    CHECK(PrefabFold::Fold(lCaptured, lEmpty, lRegistry) == 0);
    CHECK(lCaptured.EntityCount() == 2);       // still there, expanded
    CHECK(lCaptured.InstanceCount() == 0);
}

TEST_CASE("MapJson: a map with placements round-trips, and one WITHOUT is byte-identical to before")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    StubResolver     lResolver;
    lResolver.Add("Prefabs/Gun.opaaxprefab", lPrefab);

    const MapId lMap = MapId("Level01");
    World       lWorld("W");
    MapData lCaptured = PlaceAndCapture(lWorld, lPrefab, lRegistry, "Prefabs/Gun.opaaxprefab",
                                        Guid::New(), lMap);
    REQUIRE(PrefabFold::Fold(lCaptured, lResolver, lRegistry) == 1);

    const OpaaxString lText = MapJson::Serialize(lCaptured);

    MapData lParsed;
    REQUIRE(MapJson::Deserialize(lText, lParsed));
    REQUIRE(lParsed.InstanceCount() == 1);
    CHECK(lParsed.Instances[0].Prefab == OpaaxString("Prefabs/Gun.opaaxprefab"));
    CHECK(lParsed.Instances[0].InstanceId == lCaptured.Instances[0].InstanceId);

    // The fixed point MP6 gates the whole layer on.
    CHECK(MapJson::Serialize(lParsed) == lText);

    // AND the key is OMITTED when there is nothing to say, so every existing map on disk is
    // unaffected until it actually holds a placement.
    MapData lPlain;
    lPlain.Id = lMap;
    lPlain.Entities.emplace_back();
    lPlain.Entities[0].Id = Guid::New();
    CHECK(MapJson::Serialize(lPlain).Find("prefabInstances") < 0);
}

TEST_CASE("MapJson: a v1 map (no prefabInstances) still reads, with no placements")
{
    const nlohmann::json lV1{
        { MapJson::KEY_VERSION,  1u },
        { MapJson::KEY_MAP_ID,   "Old" },
        { MapJson::KEY_ENTITIES, nlohmann::json::array() }
    };

    MapData lParsed;
    REQUIRE(MapJson::FromJson(lV1, lParsed));
    CHECK(lParsed.InstanceCount() == 0);
    CHECK(lParsed.Id == MapId("Old"));
}

// =============================================================================
// The document layers
// =============================================================================
TEST_CASE("PrefabJson: round trip preserves entities, names and components")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lPrefab = MakePrefab(lRegistry);
    const OpaaxString lText  = PrefabJson::Serialize(lPrefab);

    PrefabData lParsed;
    REQUIRE(PrefabJson::Deserialize(lText, lParsed));

    REQUIRE(lParsed.EntityCount() == lPrefab.EntityCount());

    // Serializing the PARSED copy must give the same bytes — the fixed-point property MP6 gates
    // the map format on, asserted here directly.
    CHECK(PrefabJson::Serialize(lParsed) == lText);
}

TEST_CASE("PrefabJson: a NEWER format version is refused and leaves the caller's data alone")
{
    PrefabData lExisting;
    lExisting.Entities.emplace_back();
    lExisting.Entities[0].Name = OpaaxString("Kept");

    const nlohmann::json lFuture{
        { PrefabJson::KEY_VERSION,  PrefabJson::PREFAB_FORMAT_VERSION + 1 },
        { PrefabJson::KEY_ENTITIES, nlohmann::json::array() }
    };

    CHECK_FALSE(PrefabJson::FromJson(lFuture, lExisting));
    REQUIRE(lExisting.EntityCount() == 1);
    CHECK(lExisting.Entities[0].Name == OpaaxString("Kept"));
}

TEST_CASE("PrefabJson: an empty prefab is a real document, not a broken one")
{
    const nlohmann::json lEmpty{ { PrefabJson::KEY_VERSION, PrefabJson::PREFAB_FORMAT_VERSION } };

    PrefabData lParsed;
    CHECK(PrefabJson::FromJson(lEmpty, lParsed));
    CHECK(lParsed.IsEmpty());
}

TEST_CASE("PrefabFile + PrefabResource: prefab -> file -> resource -> instance")
{
    const ScopedTempDir lTemp("resource");
    const OpaaxString   lPath = lTemp.Sub("Gun.opaaxprefab");

    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const PrefabData lAuthored = MakePrefab(lRegistry);
    REQUIRE(PrefabFile::Save(lPath, lAuthored));

    // Through the ResourceManager, exactly as the engine will — which is also what buys the dedup
    // a level placing many instances of one prefab depends on.
    ResourceManager lResources;
    const ResourceRef<PrefabResource> lRef = lResources.Load<PrefabResource>(lPath.CStr());
    const PrefabResource* const       lLoaded = lRef.Get();

    REQUIRE(lLoaded != nullptr);
    REQUIRE(lLoaded->Data.EntityCount() == 2);

    World lWorld("Target");
    MapData lInstance = PrefabFactory::BuildInstance(
        lLoaded->Data, OpaaxString("Prefabs/Gun.opaaxprefab"), Guid::New(), MapId("L"), lRegistry);

    CHECK(MapFactory::Instantiate(lInstance, lWorld, lRegistry) == 2);

    lResources.FlushAll();
}

TEST_CASE("PrefabResource: a missing file resolves to NULL, never to an empty prefab")
{
    // FailFast, and the reason is that an empty prefab does not degrade, it LIES: the instantiate
    // reports success, nothing appears, and no layer says why.
    ResourceManager lResources;
    const ResourceRef<PrefabResource> lRef = lResources.Load<PrefabResource>("Nope/Missing.opaaxprefab");

    CHECK(lRef.Get() == nullptr);

    lResources.FlushAll();
}
