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
#include "World/Prefab/PrefabJson.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/MapFactory.h"
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
