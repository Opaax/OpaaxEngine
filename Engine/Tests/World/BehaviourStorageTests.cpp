// Suite: a Behaviour is stored, saved and placed like a component (map round trip, prefab overrides),
// is found with TryGet, never moves in storage, and is an object of its own class.
#include <doctest.h>

#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/ModuleRegistrar.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

// A game's namespace: the JSON must still be found for its behaviours.
namespace GameSide
{
    struct Mover final : Behaviour
    {
        OPAAX_OBJECT(Mover, Behaviour)

        float    Speed       = 1.f;
        Int32    Lives       = 3;
        Vector2F Direction   = { 1.f, 0.f };
        Int32    RuntimeHits = 0;   // not listed: runtime state, never saved

        OPAAX_PROPERTIES(Mover,
                         OPAAX_PROP(Speed),
                         OPAAX_PROP(Lives),
                         OPAAX_PROP(Direction))

        void OnUpdate(float InDeltaTime) override
        {
            ++RuntimeHits;
            Speed += InDeltaTime;
        }
    };

    // No fields at all.
    struct Spinner final : Behaviour
    {
        OPAAX_OBJECT(Spinner, Behaviour)
    };

    // Registered the way a game registers its behaviours.
    class RegisteredProbe final : public Behaviour
    {
        OPAAX_OBJECT(RegisteredProbe, Behaviour)
    };
    OPAAX_REGISTER_BEHAVIOUR(RegisteredProbe);
}

static_assert(CComponent<GameSide::Mover>,   "a behaviour must satisfy the component concept");
static_assert(CComponent<GameSide::Spinner>, "a behaviour with no fields must too");

namespace
{
    const MapId k_Map = MapId("Level01");

    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<TransformComponent>("Transform", /*bEssential*/true));
        REQUIRE(InRegistry.Register<PrefabInstanceComponent>("PrefabInstance"));

        // The game's route.
        BehaviourRoute lRoute;
        lRoute.Bind(&InRegistry);
        REQUIRE(lRoute.Register<GameSide::Mover>());
        REQUIRE(lRoute.Register<GameSide::Spinner>());
    }

    const ComponentData* FindComponent(const EntityData& InEntity, const char* InName)
    {
        for (const ComponentData& lComponent : InEntity.Components)
        {
            if (lComponent.TypeName == OpaaxStringID(InName)) { return &lComponent; }
        }
        return nullptr;
    }

    class OnePrefabResolver final : public IPrefabResolver
    {
    public:
        OnePrefabResolver(const char* InPath, const PrefabData& InData) : m_Path(InPath), m_Data(InData) {}

        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override
        {
            return InAssetPath == m_Path ? &m_Data : nullptr;
        }

    private:
        OpaaxString m_Path;
        PrefabData  m_Data;
    };
}

TEST_CASE("Behaviour: registered under its type name, as a component every generic path can see")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const IComponentEntry* lMover = lRegistry.FindByName(OpaaxStringID("Mover"));
    REQUIRE(lMover != nullptr);
    CHECK(lRegistry.Find<GameSide::Mover>() == lMover);
    CHECK(lMover->IsReflected());           // the Inspector can draw it
    CHECK_FALSE(lMover->IsEssential());     // Add / Remove like any component

    CHECK(lRegistry.FindByName(OpaaxStringID("Spinner")) != nullptr);
}

TEST_CASE("Behaviour: a map round trip keeps its listed fields, and only those")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World  lSource("Source");
    Entity lShip = lSource.CreateEntity("Ship", k_Map);

    GameSide::Mover& lMover = lShip.Add<GameSide::Mover>();
    lMover.Speed       = 4.5f;
    lMover.Lives       = 7;
    lMover.Direction   = { 0.f, -1.f };
    lMover.RuntimeHits = 99;
    lShip.Add<GameSide::Spinner>();

    const MapData lCaptured = MapSerializer::CaptureWorld(lSource, lRegistry);
    REQUIRE(lCaptured.EntityCount() == 1u);

    // The file holds the listed fields only.
    const ComponentData* lPayload = FindComponent(lCaptured.Entities[0], "Mover");
    REQUIRE(lPayload != nullptr);
    CHECK(lPayload->Payload.size() == 3);
    CHECK_FALSE(lPayload->Payload.contains("RuntimeHits"));
    REQUIRE(FindComponent(lCaptured.Entities[0], "Spinner") != nullptr);
    CHECK(FindComponent(lCaptured.Entities[0], "Spinner")->Payload == nlohmann::json::object());

    // The PIE clone shape: capture one world, instantiate into another.
    World lTarget("Target");
    REQUIRE(MapFactory::Instantiate(lCaptured, lTarget, lRegistry) == 1u);

    Entity lClone = lTarget.FindByGuid(lShip.GetGuid());
    REQUIRE(lClone.IsValid());

    GameSide::Mover* lRead = lClone.TryGet<GameSide::Mover>();
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Speed == 4.5f);
    CHECK(lRead->Lives == 7);
    CHECK(lRead->Direction == Vector2F{ 0.f, -1.f });
    CHECK(lRead->RuntimeHits == 0);         // runtime state starts fresh
    CHECK(lClone.Has<GameSide::Spinner>());
}

TEST_CASE("Behaviour: a bad value in the file keeps that field's default; the rest still loads")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    World  lSource("Source");
    Entity lShip = lSource.CreateEntity("Ship", k_Map);
    lShip.Add<GameSide::Mover>().Speed = 8.f;

    MapData lCaptured = MapSerializer::CaptureWorld(lSource, lRegistry);
    for (ComponentData& lComponent : lCaptured.Entities[0].Components)
    {
        if (lComponent.TypeName == OpaaxStringID("Mover")) { lComponent.Payload["Lives"] = "many"; }
    }

    World lTarget("Target");
    REQUIRE_NOTHROW(MapFactory::Instantiate(lCaptured, lTarget, lRegistry));

    GameSide::Mover* lRead = lTarget.FindByGuid(lShip.GetGuid()).TryGet<GameSide::Mover>();
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Lives == 3);               // default kept (and a warning logged)
    CHECK(lRead->Speed == 8.f);             // its neighbour still read
}

TEST_CASE("Behaviour: TryGet finds it, and a call through the base reaches the game's override")
{
    World  lWorld("W");
    Entity lShip = lWorld.CreateEntity("Ship", k_Map);
    lShip.Add<GameSide::Mover>();

    GameSide::Mover* lMover = lShip.TryGet<GameSide::Mover>();
    REQUIRE(lMover != nullptr);

    Behaviour& lBase = *lMover;
    lBase.OnUpdate(0.5f);

    CHECK(lMover->RuntimeHits == 1);
    CHECK(lMover->Speed == 1.5f);
    CHECK(lShip.TryGet<GameSide::Spinner>() == nullptr);
}

TEST_CASE("Behaviour: an instance never moves when another holder is destroyed or many are added")
{
    World lWorld("W");

    Entity lFirst  = lWorld.CreateEntity("First", k_Map);
    Entity lSecond = lWorld.CreateEntity("Second", k_Map);
    lFirst.Add<GameSide::Mover>();
    lSecond.Add<GameSide::Mover>().Lives = 42;

    const GameSide::Mover* const lAddress = lSecond.TryGet<GameSide::Mover>();

    // Packed storage would move the last element into the destroyed one's slot.
    lWorld.DestroyEntity(lFirst);
    CHECK(lSecond.TryGet<GameSide::Mover>() == lAddress);

    for (Int32 i = 0; i < 3000; ++i)
    {
        lWorld.CreateEntity("Filler", k_Map).Add<GameSide::Mover>();
    }
    CHECK(lSecond.TryGet<GameSide::Mover>() == lAddress);
    CHECK(lAddress->Lives == 42);
}

TEST_CASE("Behaviour: a prefab override on a behaviour field holds, and a prefab change to another field arrives")
{
    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    // The prefab: one entity carrying a Mover.
    PrefabData lPrefab;
    {
        World  lAuthoring("Authoring");
        Entity lShip = lAuthoring.CreateEntity("Ship", MapId("Source"));
        lShip.Add<GameSide::Mover>().Speed = 2.f;

        MapData lCaptured = MapSerializer::CaptureMap(lAuthoring, lRegistry, MapId("Source"));
        lPrefab.Entities  = Move(lCaptured.Entities);
        for (EntityData& lEntity : lPrefab.Entities) { lEntity.OwnerMap = MapId(); }
    }

    const char* const k_Path = "Prefabs/Ship.opaaxprefab";

    // Placed, then the author changes Speed on the placement.
    World         lWorld("W");
    const MapData lInstance = PrefabFactory::BuildInstance(lPrefab, OpaaxString(k_Path), Guid::New(), k_Map, lRegistry);
    REQUIRE(MapFactory::Instantiate(lInstance, lWorld, lRegistry) == 1);

    Entity lPlaced = lWorld.FindByGuid(lInstance.Entities[0].Id);
    REQUIRE(lPlaced.IsValid());
    lPlaced.Get<GameSide::Mover>().Speed = 9.f;

    MapData lSaved = MapSerializer::CaptureMap(lWorld, lRegistry, k_Map);
    REQUIRE(PrefabFold::Fold(lSaved, OnePrefabResolver(k_Path, lPrefab), lRegistry) == 1);
    REQUIRE(lSaved.Instances[0].Overrides.size() == 1);

    // The prefab then changes a field nobody overrode.
    for (ComponentData& lComponent : lPrefab.Entities[0].Components)
    {
        if (lComponent.TypeName == OpaaxStringID("Mover")) { lComponent.Payload["Lives"] = 10; }
    }

    REQUIRE(PrefabFold::Expand(lSaved, OnePrefabResolver(k_Path, lPrefab), lRegistry) == 1);

    // Loaded into a fresh world, through the behaviour's own JSON.
    World lReloaded("Reloaded");
    REQUIRE(MapFactory::Instantiate(lSaved, lReloaded, lRegistry) >= 1);

    GameSide::Mover* lMover = lReloaded.FindByGuid(lPlaced.GetGuid()).TryGet<GameSide::Mover>();
    REQUIRE(lMover != nullptr);
    CHECK(lMover->Speed == 9.f);            // the override held
    CHECK(lMover->Lives == 10);             // the prefab's change arrived
}

TEST_CASE("Behaviour: an object of its own class, which a weak pointer follows until its entity goes")
{
    World lWorld("W");

    Entity           lShip  = lWorld.CreateEntity("Ship", k_Map);
    GameSide::Mover& lMover = lShip.Add<GameSide::Mover>();

    CHECK(&lMover.GetClass() == &GameSide::Mover::StaticClass());
    CHECK(lMover.IsA<Behaviour>());
    CHECK(GameSide::Mover::StaticClass().GetName() == OpaaxStringID("Mover"));

    // Its properties as the class lists them: what the map saves under the same name.
    TDynArray<OpaaxString> lNames;
    for (const char* lName : GameSide::Mover::StaticClass().GetPropertyNames()) { lNames.emplace_back(lName); }
    REQUIRE(lNames.size() == 3u);
    CHECK(lNames[0] == "Speed");

    // The handle follows the behaviour where it lives, and reads null once its entity is destroyed.
    const TWeakObjectPtr<GameSide::Mover> lWeak(&lMover);
    for (Int32 i = 0; i < 100; ++i)
    {
        lWorld.CreateEntity("Filler", k_Map).Add<GameSide::Mover>();
    }
    CHECK(lWeak.Get() == lShip.TryGet<GameSide::Mover>());

    lWorld.DestroyEntity(lShip);
    CHECK(lWeak.Get() == nullptr);
}

TEST_CASE("Behaviour: the registration line registers its class as well as its storage")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    const ObjectClass* lClass = lRegistries.ObjectClasses().FindByName(OpaaxStringID("RegisteredProbe"));
    REQUIRE(lClass != nullptr);
    CHECK(lClass == &GameSide::RegisteredProbe::StaticClass());
    CHECK(lClass->IsChildOf(Behaviour::StaticClass()));

    const IComponentEntry* lEntry = lRegistries.Components().FindByName(OpaaxStringID("RegisteredProbe"));
    REQUIRE(lEntry != nullptr);
    CHECK(lEntry->IsBehaviour());
}
