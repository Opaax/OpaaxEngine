// Suite: ComponentRegistry v2 — registration, refusal rules, sealing, and the type-erased
// entry's Has/Add/Save/Load.
//
// The registry is INSTANCE-owned (I1) — the retired Legacy/ECS/ComponentRegistry was entirely
// static, which is the part deliberately not salvaged. Every case here builds its own registry
// or its own bare WorldManager, so nothing shares state.
#include <doctest.h>

#include <entt/entt.hpp>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Engine.h"

using namespace Opaax;

namespace
{
    // A component defined HERE, in the test exe — the same position a game module is in.
    // Satisfying CComponent is its whole contract: no base class, no engine registration.
    struct ProbeComponent
    {
        int         Health = 0;
        float       Speed  = 0.f;
        OpaaxString Tag;

        bool operator==(const ProbeComponent& InOther) const
        {
            return Health == InOther.Health && Speed == InOther.Speed && Tag == InOther.Tag;
        }
    };

    inline void to_json(nlohmann::json& InJson, const ProbeComponent& InValue)
    {
        InJson = nlohmann::json{
            {"Health", InValue.Health},
            {"Speed", InValue.Speed},
            {"Tag", std::string(InValue.Tag.CStr())}
        };
    }

    inline void from_json(const nlohmann::json& InJson, ProbeComponent& InValue)
    {
        InJson.at("Health").get_to(InValue.Health);
        InJson.at("Speed").get_to(InValue.Speed);
        InValue.Tag = OpaaxString(InJson.at("Tag").get<std::string>().c_str());
    }

    // Concept-satisfaction is a COMPILE-time gate, so it is asserted at compile time. A type
    // without the json pair simply won't instantiate Register<T> — no runtime case can show that.
    static_assert(CComponent<ProbeComponent>, "ProbeComponent must satisfy CComponent");
    static_assert(CComponent<DummyComponent>, "DummyComponent must satisfy CComponent");
    static_assert(!CComponent<EntityMeta>,
                  "EntityMeta is identity, not user data — the snapshot core writes it by hand "
                  "and it must NOT be registrable as an ordinary component.");
}

// =============================================================================
// Registration
// =============================================================================
TEST_CASE("ComponentRegistry: a registered type is findable by name and by type id")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    CHECK(lRegistry.Count() == 1u);

    const IComponentEntry* lByName = lRegistry.FindByName(OpaaxStringID("Probe"));
    REQUIRE(lByName != nullptr);

    const IComponentEntry* lById = lRegistry.FindByTypeId(entt::type_hash<ProbeComponent>::value());
    REQUIRE(lById != nullptr);

    // Both lookups must land on the SAME entry — two entries for one type would mean the
    // round trip could pick either.
    CHECK(lByName == lById);
    CHECK(lByName->GetName() == OpaaxStringID("Probe"));
}

TEST_CASE("ComponentRegistry: an unregistered type resolves to nullptr, not to something else")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));

    CHECK(lRegistry.FindByName(OpaaxStringID("NeverRegistered")) == nullptr);
    CHECK(lRegistry.FindByTypeId(entt::type_hash<DummyComponent>::value()) == nullptr);
}

TEST_CASE("ComponentRegistry: ForEach visits every entry in registration order")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    REQUIRE(lRegistry.Register<DummyComponent>("Dummy"));

    TDynArray<OpaaxStringID> lSeen;
    lRegistry.ForEach([&](const IComponentEntry& InEntry) { lSeen.push_back(InEntry.GetName()); });

    REQUIRE(lSeen.size() == 2u);
    CHECK(lSeen[0] == OpaaxStringID("Probe"));
    CHECK(lSeen[1] == OpaaxStringID("Dummy"));
}

// =============================================================================
// Refusal rules — each one exists to stop a specific silent corruption
// =============================================================================
TEST_CASE("ComponentRegistry: a duplicate NAME is refused (it would make a map file ambiguous)")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Taken"));
    CHECK_FALSE(lRegistry.Register<DummyComponent>("Taken"));

    // The first registration must survive intact — a refusal is not a replacement.
    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistry.FindByName(OpaaxStringID("Taken"))->GetTypeId()
          == entt::type_hash<ProbeComponent>::value());
}

TEST_CASE("ComponentRegistry: registering the same TYPE twice is refused")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("First"));
    CHECK_FALSE(lRegistry.Register<ProbeComponent>("Second"));

    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistry.FindByName(OpaaxStringID("Second")) == nullptr);
}

TEST_CASE("ComponentRegistry: an empty name is refused")
{
    ComponentRegistry lRegistry;

    CHECK_FALSE(lRegistry.Register<ProbeComponent>(OpaaxStringID{}));
    CHECK(lRegistry.Count() == 0u);
}

// =============================================================================
// Sealing — Editor.md §3 L1
// =============================================================================
TEST_CASE("ComponentRegistry: Seal is idempotent and refuses every later registration")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    CHECK_FALSE(lRegistry.IsSealed());

    lRegistry.Seal();
    lRegistry.Seal(); // idempotent — a second world must not be a second seal event
    CHECK(lRegistry.IsSealed());

    // The whole point: a type accepted now would be missing from every entity in the world
    // that already exists, with no error anywhere.
    CHECK_FALSE(lRegistry.Register<DummyComponent>("TooLate"));
    CHECK(lRegistry.Count() == 1u);
}

TEST_CASE("WorldManager: the registries seal at the FIRST CreateWorld, not before")
{
    // The registries are the ENGINE's; WorldManager only borrows them to seal on its way to
    // the first world. A bare manager takes them by pointer, which is what makes that testable
    // without standing up an Engine.
    EngineRegistries lRegistries;
    WorldManager     lManager(&lRegistries);

    REQUIRE(lRegistries.Components().Register<ProbeComponent>("Probe"));
    CHECK_FALSE(lRegistries.Components().IsSealed());

    lManager.CreateWorld("First");
    CHECK(lRegistries.Components().IsSealed());

    lManager.CreateWorld("Second"); // still fine — SealAll is idempotent
    CHECK(lRegistries.Components().IsSealed());

    // And the seal means what it says.
    CHECK_FALSE(lRegistries.Components().Register<DummyComponent>("TooLate"));
}

TEST_CASE("WorldManager: a manager with no registries still creates worlds")
{
    // Null registries is the bare-test case. It must not crash on the seal path — the whole
    // point of making the borrow explicit rather than assumed.
    WorldManager lManager;

    CHECK(lManager.GetRegistries() == nullptr);
    CHECK(lManager.CreateWorld("Orphan") != nullptr);
    CHECK(lManager.GetWorldCount() == 1u);
}

TEST_CASE("Engine: DummyComponent is registered natively, DLL-side, and found exe-side")
{
    // Natives are the ENGINE's job now, done in its ctor before any subsystem exists (MR2).
    // Constructing an Engine only queues subsystem factories + registers natives — nothing
    // starts, no service is touched.
    Engine lEngine;

    // The cross-boundary statement: RegisterNativeTypes runs inside the DLL (Engine.cpp), the
    // type id below is computed HERE in the exe. A mismatch returns null — see
    // ComponentIdentityTests.cpp for why this can be trusted (I2).
    const IComponentEntry* lEntry =
        lEngine.GetRegistries().Components().FindByTypeId(entt::type_hash<DummyComponent>::value());

    REQUIRE(lEntry != nullptr);
    CHECK(lEntry->GetName() == OpaaxStringID("Dummy"));
}

// =============================================================================
// The type-erased entry — Has / Add / Save / Load
// =============================================================================
TEST_CASE("ComponentEntry: Save/Load round-trips a component's values through json")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));

    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));
    REQUIRE(lEntry != nullptr);

    World  lWorld("EntryTest");
    Entity lSource = lWorld.CreateEntity("Source");
    Entity lTarget = lWorld.CreateEntity("Target");

    const ProbeComponent lOriginal{ 42, 3.5f, OpaaxString("hero") };
    lSource.Add<ProbeComponent>(lOriginal);

    EntityRegistry& lReg = lWorld.GetRegistry();

    CHECK(lEntry->Has(lReg, lSource.GetHandle()));
    CHECK_FALSE(lEntry->Has(lReg, lTarget.GetHandle()));

    const nlohmann::json lJson = lEntry->Save(lReg, lSource.GetHandle());
    lEntry->Load(lReg, lTarget.GetHandle(), lJson);

    REQUIRE(lTarget.Has<ProbeComponent>());
    CHECK(lTarget.Get<ProbeComponent>() == lOriginal);
}

TEST_CASE("ComponentEntry: Load ADDS the component when the target doesn't have it yet")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Fresh");

    // This is the instantiate path: the entity is brand new and carries nothing.
    REQUIRE_FALSE(lEntity.Has<ProbeComponent>());

    lEntry->Load(lWorld.GetRegistry(), lEntity.GetHandle(),
                 nlohmann::json{{"Health", 7}, {"Speed", 1.25f}, {"Tag", "spawned"}});

    REQUIRE(lEntity.Has<ProbeComponent>());
    CHECK(lEntity.Get<ProbeComponent>().Health == 7);
    CHECK(lEntity.Get<ProbeComponent>().Tag == "spawned");
}

TEST_CASE("ComponentEntry: Add default-constructs, and is a no-op when already present")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Subject");

    lEntry->Add(lWorld.GetRegistry(), lEntity.GetHandle());
    REQUIRE(lEntity.Has<ProbeComponent>());
    CHECK(lEntity.Get<ProbeComponent>().Health == 0);

    lEntity.Get<ProbeComponent>().Health = 99;

    // Adding again must not wipe the edit — the "Add Component" menu path must be safe to
    // press twice.
    lEntry->Add(lWorld.GetRegistry(), lEntity.GetHandle());
    CHECK(lEntity.Get<ProbeComponent>().Health == 99);
}

TEST_CASE("ComponentEntry: Save on an entity without the component yields a null json")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Empty");

    // Capture asks Has() before Save(), but a null return keeps the entry honest on its own.
    CHECK(lEntry->Save(lWorld.GetRegistry(), lEntity.GetHandle()).is_null());
}
