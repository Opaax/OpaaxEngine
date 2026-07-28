// Suite: the Components() module route — the seam a game module registers through (D9).
//
// This is the M3 gate's "including module components" half. The test exe sits in exactly the
// position a game module does: the component types below are defined HERE, while the
// ComponentRegistry that stores them and the entt registry they land in live in the DLL.
#include <doctest.h>

#include "Engine/Modules/ModuleRegistrar.h"
#include "World/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace TestGame
{
    // A named namespace on purpose: the derived name must be the LEAF, so registering this
    // yields "AmmoComponent" and not "TestGame::AmmoComponent".
    struct AmmoComponent
    {
        int Rounds = 0;

        bool operator==(const AmmoComponent& InOther) const { return Rounds == InOther.Rounds; }
    };

    inline void to_json(nlohmann::json& InJson, const AmmoComponent& InValue)
    {
        InJson = nlohmann::json{{"Rounds", InValue.Rounds}};
    }

    inline void from_json(const nlohmann::json& InJson, AmmoComponent& InValue)
    {
        InJson.at("Rounds").get_to(InValue.Rounds);
    }

    struct ShieldComponent
    {
        float Strength = 0.f;
    };

    inline void to_json(nlohmann::json& InJson, const ShieldComponent& InValue)
    {
        InJson = nlohmann::json{{"Strength", InValue.Strength}};
    }

    inline void from_json(const nlohmann::json& InJson, ShieldComponent& InValue)
    {
        InJson.at("Strength").get_to(InValue.Strength);
    }
}

// =============================================================================
// Binding
// =============================================================================
TEST_CASE("ComponentRoute: an UNBOUND route refuses rather than silently dropping")
{
    ModuleRegistrar lRegistrar; // never bound — i.e. EngineStartup forgot to wire it

    // The failure this guards: a wiring bug would otherwise swallow every module component
    // and only surface much later, as entities missing components nobody can explain.
    CHECK_FALSE(lRegistrar.Components().Register<TestGame::AmmoComponent>());

    // The attempt is still counted, so a registrar reporting "1 registered" against a
    // registry holding 0 is visibly inconsistent.
    CHECK(lRegistrar.Components().Count() == 1u);
}

TEST_CASE("ComponentRoute: a bound route forwards into the registry")
{
    ComponentRegistry lRegistry;
    ModuleRegistrar   lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistry);

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());

    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistrar.Components().Count() == 1u);
}

// =============================================================================
// The derived name — MR1's call site survives because the name is optional
// =============================================================================
TEST_CASE("ComponentRoute: an omitted name derives the type's LEAF name")
{
    ComponentRegistry lRegistry;
    ModuleRegistrar   lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistry);

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());

    // Namespace-stripped: the key written into a map file must not carry C++ scoping.
    CHECK(lRegistry.FindByName(OpaaxStringID("AmmoComponent")) != nullptr);
    CHECK(lRegistry.FindByName(OpaaxStringID("TestGame::AmmoComponent")) == nullptr);
}

TEST_CASE("ComponentRoute: an explicit name overrides the derived one")
{
    ComponentRegistry lRegistry;
    ModuleRegistrar   lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistry);

    // Pinning the name is how a game keeps saved maps loadable across a C++ rename.
    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>(OpaaxStringID("Ammo")));

    CHECK(lRegistry.FindByName(OpaaxStringID("Ammo")) != nullptr);
    CHECK(lRegistry.FindByName(OpaaxStringID("AmmoComponent")) == nullptr);
}

TEST_CASE("ComponentRoute: Count records refusals too")
{
    ComponentRegistry lRegistry;
    ModuleRegistrar   lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistry);

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());
    CHECK_FALSE(lRegistrar.Components().Register<TestGame::AmmoComponent>()); // duplicate type

    CHECK(lRegistrar.Components().Count() == 2u); // asked twice
    CHECK(lRegistry.Count() == 1u);               // accepted once
}

TEST_CASE("ComponentRoute: WorldSubsystems is still counts-only (its registry lands in M4)")
{
    ModuleRegistrar lRegistrar;

    lRegistrar.WorldSubsystems().Register<int>();
    CHECK(lRegistrar.WorldSubsystems().Count() == 1u);
}

// =============================================================================
// The dogfood — a module component through the full snapshot core
// =============================================================================
TEST_CASE("Module components round-trip through capture -> instantiate, GUIDs preserved")
{
    ComponentRegistry lRegistry;
    ModuleRegistrar   lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistry);

    // Exactly what SandboxModule::OnRegister does — two types the engine has never heard of.
    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());
    REQUIRE(lRegistrar.Components().Register<TestGame::ShieldComponent>());

    const MapId lMap = MapId("Level01");

    World  lWorld("ModuleDogfood");
    Entity lShip = lWorld.CreateEntity("Ship", lMap);
    lShip.Add<TestGame::AmmoComponent>(TestGame::AmmoComponent{250});
    lShip.Add<TestGame::ShieldComponent>(TestGame::ShieldComponent{0.75f});

    const Guid lGuid = lShip.GetGuid();

    const MapData lCaptured = MapSerializer::Capture(lWorld, lRegistry, lMap);
    REQUIRE(lCaptured.EntityCount() == 1u);
    CHECK(lCaptured.Entities[0].Components.size() == 2u);

    lWorld.Clear();
    REQUIRE(MapFactory::Instantiate(lCaptured, lWorld, lRegistry) == 1u);

    Entity lRestored = lWorld.FindByGuid(lGuid);
    REQUIRE(lRestored.IsValid());
    REQUIRE(lRestored.Has<TestGame::AmmoComponent>());
    CHECK(lRestored.Get<TestGame::AmmoComponent>() == TestGame::AmmoComponent{250});
    REQUIRE(lRestored.Has<TestGame::ShieldComponent>());
    CHECK(lRestored.Get<TestGame::ShieldComponent>().Strength == doctest::Approx(0.75f));
}
