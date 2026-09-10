// Suite: ⑦-C P7 — nesting + variants, the headless half.
//
// A prefab file may hold PLACEMENT RECORDS (the map's own, PF3) and every consumer reads the
// prefab FLATTENED through the resolver: its own entities plus every record expanded, nested guids
// `Derive(record.InstanceId, template)`. A VARIANT is a file with no entities and one record. The
// stub resolver below flattens the way ResourcePrefabResolver does — cache, in-flight chain, cycle
// refused — so these cases exercise the real composition, not a shortcut.
#include <doctest.h>

#include "Engine/Subsystems/Resources/ResourceManager.h"   // before PrefabResource — completes LoadContext
#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabJson.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<TransformComponent>("Transform", /*bEssential*/ true));
        REQUIRE(InRegistry.Register<PrefabInstanceComponent>("PrefabInstance"));
    }

    // One entity with a transform at InX, guid minted here so a case can name it.
    EntityData Piece(const char* InName, const float InX, const Guid& InId = Guid::New())
    {
        TransformComponent lXf;
        lXf.Position = Vector2F{ InX, 0.f };

        EntityData lEntity;
        lEntity.Id   = InId;
        lEntity.Name = OpaaxString(InName);
        lEntity.Components.emplace_back(OpaaxStringID("Transform"), nlohmann::json(lXf));
        return lEntity;
    }

    PrefabInstanceRecord Placement(const char* InPath, const Guid& InInstanceId)
    {
        return PrefabInstanceRecord{ OpaaxString(InPath), InInstanceId, {} };
    }

    float XOf(const EntityData& InEntity)
    {
        for (const ComponentData& lComponent : InEntity.Components)
        {
            if (lComponent.TypeName == OpaaxStringID("Transform"))
            {
                return lComponent.Payload.get<TransformComponent>().Position.x;
            }
        }
        return -9999.f;
    }

    bool HasMarker(const EntityData& InEntity)
    {
        for (const ComponentData& lComponent : InEntity.Components)
        {
            if (lComponent.TypeName == OpaaxStringID("PrefabInstance")) { return true; }
        }
        return false;
    }

    // ResourcePrefabResolver's shape without the ResourceManager: raw files by path, flattened on
    // first sight through Flatten, an in-flight flag per entry, a cycle refused with a null.
    class FlatteningStub final : public IPrefabResolver
    {
    public:
        explicit FlatteningStub(const ComponentRegistry& InRegistry) : m_Registry(InRegistry) {}

        void Add(const char* InPath, PrefabData InRaw)
        {
            m_Entries.emplace_back(MakeUnique<Entry>(Entry{ OpaaxString(InPath), Move(InRaw), nullptr, false }));
        }

        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override
        {
            for (const TUniquePtr<Entry>& lEntry : m_Entries)
            {
                if (lEntry->Path != InAssetPath) { continue; }

                if (lEntry->bInFlight) { ++Cycles; return nullptr; }

                if (lEntry->Flattened == nullptr)
                {
                    lEntry->bInFlight  = true;
                    lEntry->Flattened  = MakeUnique<PrefabData>(
                        PrefabFactory::Flatten(lEntry->Raw, lEntry->Path, *this, m_Registry));
                    lEntry->bInFlight  = false;
                }

                return lEntry->Flattened.get();
            }
            return nullptr;
        }

        mutable Uint64 Cycles = 0;   // how many times a placement closed a loop

    private:
        struct Entry
        {
            OpaaxString            Path;
            PrefabData             Raw;
            TUniquePtr<PrefabData> Flattened;
            bool                   bInFlight;
        };

        const ComponentRegistry&      m_Registry;
        TDynArray<TUniquePtr<Entry>>  m_Entries;
    };
}

// =============================================================================
// The format
// =============================================================================

TEST_CASE("PrefabJson v2: a record round-trips byte-exact, and a file placing nothing carries no key")
{
    const Guid lTemplate = Guid::New();
    const Guid lInstance = Guid::New();

    PrefabData lData;
    lData.Entities.emplace_back(Piece("Own", 1.f));

    PrefabInstanceRecord lRecord = Placement("Prefabs/Inner.opaaxprefab", lInstance);
    lRecord.Overrides.emplace_back(PrefabOverrideEntry{ lTemplate, nlohmann::json{ { "components", { { "Transform", { { "Position", { { "x", 5.0 } } } } } } } } });
    lData.Instances.emplace_back(Move(lRecord));

    const OpaaxString lText = PrefabJson::Serialize(lData);
    CHECK(lText.Find("prefabInstances") >= 0);

    PrefabData lBack;
    REQUIRE(PrefabJson::Deserialize(lText, lBack));
    REQUIRE(lBack.Instances.size() == 1);
    CHECK(lBack.Instances[0].Prefab     == OpaaxString("Prefabs/Inner.opaaxprefab"));
    CHECK(lBack.Instances[0].InstanceId == lInstance);
    REQUIRE(lBack.Instances[0].Overrides.size() == 1);
    CHECK(lBack.Instances[0].Overrides[0].TemplateGuid == lTemplate);

    // Byte-exact: the reader and the writer agree, which is the gate MP6 stands on.
    CHECK(PrefabJson::Serialize(lBack) == lText);

    // No records => no key: a prefab placing nothing is what it was before P7, but for the version.
    PrefabData lPlain;
    lPlain.Entities.emplace_back(Piece("Only", 0.f));
    CHECK(PrefabJson::Serialize(lPlain).Find("prefabInstances") < 0);
}

TEST_CASE("PrefabJson: a v1 file reads as a prefab with no placements")
{
    const OpaaxString lV1(R"({"version":1,"entities":[{"guid":"0123456789abcdef0123456789abcdef","name":"Old","ownerMap":"","components":{}}]})");

    PrefabData lData;
    REQUIRE(PrefabJson::Deserialize(lV1, lData));
    CHECK(lData.EntityCount()   == 1);
    CHECK(lData.InstanceCount() == 0);
}

// =============================================================================
// Flatten
// =============================================================================

TEST_CASE("Flatten: an outer prefab is its own entities plus every nested placement, on derived guids")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const Guid lInnerA = Guid::New();
    const Guid lInnerB = Guid::New();
    const Guid lRecord = Guid::New();

    PrefabData lInner;
    lInner.Entities.emplace_back(Piece("InnerA", 10.f, lInnerA));
    lInner.Entities.emplace_back(Piece("InnerB", 20.f, lInnerB));

    PrefabData lOuter;
    lOuter.Entities.emplace_back(Piece("Own", 1.f));
    PrefabInstanceRecord lPlacement = Placement("Prefabs/Inner.opaaxprefab", lRecord);
    // The outer prefab moved the inner's B: an override keyed by the INNER's template guid.
    lPlacement.Overrides.emplace_back(PrefabOverrideEntry{ lInnerB, nlohmann::json{ { "components", { { "Transform", { { "Position", { { "x", 99.0 } } } } } } } } });
    lOuter.Instances.emplace_back(Move(lPlacement));

    FlatteningStub lResolver(lRegistry);
    lResolver.Add("Prefabs/Inner.opaaxprefab", lInner);
    lResolver.Add("Prefabs/Outer.opaaxprefab", lOuter);

    const PrefabData* lFlat = lResolver.Resolve(OpaaxString("Prefabs/Outer.opaaxprefab"));
    REQUIRE(lFlat != nullptr);
    REQUIRE(lFlat->EntityCount()   == 3);
    CHECK(lFlat->InstanceCount() == 0);   // flat means flat

    // The nested pieces sit on Derive(record, template) — authored ids, stable across sessions —
    // with the override applied and NO marker left behind.
    Uint64 lFound = 0;
    for (const EntityData& lEntity : lFlat->Entities)
    {
        CHECK_FALSE(HasMarker(lEntity));
        CHECK(lEntity.OwnerMap.IsValid() == false);

        if (lEntity.Id == Guid::Derive(lRecord, lInnerA)) { ++lFound; CHECK(XOf(lEntity) == doctest::Approx(10.f)); }
        if (lEntity.Id == Guid::Derive(lRecord, lInnerB)) { ++lFound; CHECK(XOf(lEntity) == doctest::Approx(99.f)); }
    }
    CHECK(lFound == 2);
    CHECK(lResolver.Cycles == 0);
}

TEST_CASE("Flatten: two placements of an outer prefab are six distinct entities, nested guids composed")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const Guid lInnerA = Guid::New();
    const Guid lRecord = Guid::New();

    PrefabData lInner;
    lInner.Entities.emplace_back(Piece("InnerA", 10.f, lInnerA));
    lInner.Entities.emplace_back(Piece("InnerB", 20.f));

    PrefabData lOuter;
    lOuter.Entities.emplace_back(Piece("Own", 1.f));
    lOuter.Instances.emplace_back(Placement("Prefabs/Inner.opaaxprefab", lRecord));

    FlatteningStub lResolver(lRegistry);
    lResolver.Add("Prefabs/Inner.opaaxprefab", lInner);
    lResolver.Add("Prefabs/Outer.opaaxprefab", lOuter);

    // A map placing the outer twice — the level's own path through Expand.
    const Guid lPlaceOne = Guid::New();
    const Guid lPlaceTwo = Guid::New();

    MapData lMap;
    lMap.Id = MapId("Level");
    lMap.Instances.emplace_back(Placement("Prefabs/Outer.opaaxprefab", lPlaceOne));
    lMap.Instances.emplace_back(Placement("Prefabs/Outer.opaaxprefab", lPlaceTwo));

    CHECK(PrefabFold::Expand(lMap, lResolver, lRegistry) == 2);
    REQUIRE(lMap.EntityCount() == 6);

    // All distinct, and the nested one is PF2's composition: Derive(placement, Derive(record, tmpl)).
    for (Uint64 lI = 0; lI < 6; ++lI)
    {
        for (Uint64 lJ = lI + 1; lJ < 6; ++lJ) { CHECK(lMap.Entities[lI].Id != lMap.Entities[lJ].Id); }
    }

    Uint64 lComposed = 0;
    for (const EntityData& lEntity : lMap.Entities)
    {
        if (lEntity.Id == Guid::Derive(lPlaceOne, Guid::Derive(lRecord, lInnerA))) { ++lComposed; }
        if (lEntity.Id == Guid::Derive(lPlaceTwo, Guid::Derive(lRecord, lInnerA))) { ++lComposed; }
    }
    CHECK(lComposed == 2);

    // And a level placement carries the OUTER marker on every entity, nested ones included: the
    // level sees a flat set, and its template guid is the in-prefab (derived) one.
    for (const EntityData& lEntity : lMap.Entities)
    {
        REQUIRE(HasMarker(lEntity));
    }
}

TEST_CASE("Flatten: a prefab that places itself through another is refused once and terminates")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    PrefabData lA;
    lA.Entities.emplace_back(Piece("A", 1.f));
    lA.Instances.emplace_back(Placement("Prefabs/B.opaaxprefab", Guid::New()));

    PrefabData lB;
    lB.Entities.emplace_back(Piece("B", 2.f));
    lB.Instances.emplace_back(Placement("Prefabs/A.opaaxprefab", Guid::New()));   // the loop

    FlatteningStub lResolver(lRegistry);
    lResolver.Add("Prefabs/A.opaaxprefab", lA);
    lResolver.Add("Prefabs/B.opaaxprefab", lB);

    const PrefabData* lFlat = lResolver.Resolve(OpaaxString("Prefabs/A.opaaxprefab"));
    REQUIRE(lFlat != nullptr);

    // A's own + B's own; the placement that closes the loop expanded to nothing. ONE refusal.
    CHECK(lFlat->EntityCount() == 2);
    CHECK(lResolver.Cycles == 1);
}

TEST_CASE("Flatten: a VARIANT is its base with the overrides applied, on derived guids")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const Guid lBaseA  = Guid::New();
    const Guid lRecord = Guid::New();

    PrefabData lBase;
    lBase.Entities.emplace_back(Piece("BaseA", 10.f, lBaseA));
    lBase.Entities.emplace_back(Piece("BaseB", 20.f));

    // No entities of its own, one record: "that prefab, but A at x = 42".
    PrefabData lVariant;
    PrefabInstanceRecord lOver = Placement("Prefabs/Base.opaaxprefab", lRecord);
    lOver.Overrides.emplace_back(PrefabOverrideEntry{ lBaseA, nlohmann::json{ { "components", { { "Transform", { { "Position", { { "x", 42.0 } } } } } } } } });
    lVariant.Instances.emplace_back(Move(lOver));

    FlatteningStub lResolver(lRegistry);
    lResolver.Add("Prefabs/Base.opaaxprefab", lBase);
    lResolver.Add("Prefabs/Variant.opaaxprefab", lVariant);

    const PrefabData* lFlat = lResolver.Resolve(OpaaxString("Prefabs/Variant.opaaxprefab"));
    REQUIRE(lFlat != nullptr);
    REQUIRE(lFlat->EntityCount() == 2);

    bool lSawA = false;
    for (const EntityData& lEntity : lFlat->Entities)
    {
        if (lEntity.Id == Guid::Derive(lRecord, lBaseA))
        {
            lSawA = true;
            CHECK(XOf(lEntity) == doctest::Approx(42.f));   // the variant's override
        }
        else
        {
            CHECK(XOf(lEntity) == doctest::Approx(20.f));   // the base's value, untouched
        }
    }
    CHECK(lSawA);

    // Placed twice in a level, a variant is two distinct instances like any prefab.
    MapData lMap;
    lMap.Id = MapId("Level");
    lMap.Instances.emplace_back(Placement("Prefabs/Variant.opaaxprefab", Guid::New()));
    lMap.Instances.emplace_back(Placement("Prefabs/Variant.opaaxprefab", Guid::New()));
    CHECK(PrefabFold::Expand(lMap, lResolver, lRegistry) == 2);
    REQUIRE(lMap.EntityCount() == 4);
    CHECK(lMap.Entities[0].Id != lMap.Entities[2].Id);
}

TEST_CASE("Fold/Expand at the level: an override on a NESTED entity keys by its in-prefab guid and survives the round trip")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const Guid lInnerA = Guid::New();
    const Guid lRecord = Guid::New();

    PrefabData lInner;
    lInner.Entities.emplace_back(Piece("InnerA", 10.f, lInnerA));

    PrefabData lOuter;
    lOuter.Entities.emplace_back(Piece("Own", 1.f));
    lOuter.Instances.emplace_back(Placement("Prefabs/Inner.opaaxprefab", lRecord));

    FlatteningStub lResolver(lRegistry);
    lResolver.Add("Prefabs/Inner.opaaxprefab", lInner);
    lResolver.Add("Prefabs/Outer.opaaxprefab", lOuter);

    // Place the outer in a world, move the NESTED piece, capture, fold.
    World lWorld("Level");
    MapData lMap;
    lMap.Id = MapId("Level");
    const Guid lPlacement = Guid::New();
    lMap.Instances.emplace_back(Placement("Prefabs/Outer.opaaxprefab", lPlacement));
    REQUIRE(PrefabFold::Expand(lMap, lResolver, lRegistry) == 1);
    REQUIRE(MapFactory::Instantiate(lMap, lWorld, lRegistry) == 2);

    const Guid lNestedInLevel = Guid::Derive(lPlacement, Guid::Derive(lRecord, lInnerA));
    Entity lNested = lWorld.FindByGuid(lNestedInLevel);
    REQUIRE(lNested.IsValid());
    lNested.Get<TransformComponent>().Position.x = 77.f;

    MapData lCaptured = MapSerializer::CaptureMap(lWorld, lRegistry, MapId("Level"));
    REQUIRE(PrefabFold::Fold(lCaptured, lResolver, lRegistry) == 1);
    REQUIRE(lCaptured.Instances.size() == 1);
    REQUIRE(lCaptured.Instances[0].Overrides.size() == 1);

    // Keyed by the in-prefab guid of the nested piece — Derive(record, template) — never by the
    // inner prefab's own template, which the level has no way to name.
    CHECK(lCaptured.Instances[0].Overrides[0].TemplateGuid == Guid::Derive(lRecord, lInnerA));

    // And back: expand the folded record into a fresh world, the move is still there.
    World lAgain("Again");
    REQUIRE(PrefabFold::Expand(lCaptured, lResolver, lRegistry) == 1);
    REQUIRE(MapFactory::Instantiate(lCaptured, lAgain, lRegistry) == 2);
    Entity lBack = lAgain.FindByGuid(lNestedInLevel);
    REQUIRE(lBack.IsValid());
    CHECK(lBack.Get<TransformComponent>().Position.x == doctest::Approx(77.f));
}
