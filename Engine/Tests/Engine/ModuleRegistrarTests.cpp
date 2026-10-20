// Suite: ModuleRegistrar routes — how a game module registers its types. The types below are
// defined in the test exe, like a game module; the registries live in the engine library.
#include <doctest.h>

#include "Core/OpaaxTypes.h"

#include "Engine/Registries/ModuleRegistrar.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
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

    // A module world subsystem: an Object class defined here like a game type.
    class PatrolSubsystem : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(PatrolSubsystem, Opaax::WorldSubsystem)
    };

    // A module's resource type: it names the extension it claims, and the engine's table is what
    // makes `.pickup` a known file everywhere — no engine header mentions it.
    struct PickupResource
    {
        OPAAX_RESOURCE_FORMAT("Pickup Table", ".pickup")

        static constexpr Opaax::EFailPolicy FailPolicy = Opaax::EFailPolicy::Placeholder;

        static TOptional<PickupResource> Load(const char*, Opaax::LoadContext&) { return PickupResource{}; }
        static PickupResource            Placeholder() { return PickupResource{}; }
    };
}

// Global namespace on purpose (like an editor module): MSVC's "class " prefix must be stripped.
struct GlobalScopeComponent
{
    int Value = 0;
};

inline void to_json(nlohmann::json& InJson, const GlobalScopeComponent& InValue)
{
    InJson = nlohmann::json{{"Value", InValue.Value}};
}

inline void from_json(const nlohmann::json& InJson, GlobalScopeComponent& InValue)
{
    InJson.at("Value").get_to(InValue.Value);
}

class GlobalScopeSubsystem : public Opaax::WorldSubsystem
{
    OPAAX_OBJECT(GlobalScopeSubsystem, Opaax::WorldSubsystem)
};

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
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    ComponentRegistry& lRegistry = lRegistries.Components();

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());

    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistrar.Components().Count() == 1u);
}

// =============================================================================
// The derived name
// =============================================================================
TEST_CASE("ComponentRoute: an omitted name derives the type's LEAF name")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    ComponentRegistry& lRegistry = lRegistries.Components();

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());

    // Namespace-stripped: the key written into a map file must not carry C++ scoping.
    CHECK(lRegistry.FindByName(OpaaxStringID("AmmoComponent")) != nullptr);
    CHECK(lRegistry.FindByName(OpaaxStringID("TestGame::AmmoComponent")) == nullptr);
}

TEST_CASE("ComponentRoute: a GLOBAL-namespace type derives a bare name, with no 'class' keyword")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    // MSVC's type_name is "class Foo"; for a namespaced type the "::" strip hid it, so only a
    // global-namespace type shows whether "class " is stripped.
    REQUIRE(lRegistrar.Components().Register<GlobalScopeComponent>());

    CHECK(lRegistries.Components().FindByName(OpaaxStringID("GlobalScopeComponent")) != nullptr);
    CHECK(lRegistries.Components().FindByName(OpaaxStringID("class GlobalScopeComponent")) == nullptr);
    CHECK(lRegistries.Components().FindByName(OpaaxStringID("struct GlobalScopeComponent")) == nullptr);
}

TEST_CASE("ObjectClassRoute: a GLOBAL-namespace subsystem class is named bare too")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    // The shape an editor module registers: a class, at global scope, through a route.
    REQUIRE(lRegistrar.ObjectClasses().Register<GlobalScopeSubsystem>());

    CHECK(lRegistries.ObjectClasses().FindByName(OpaaxStringID("GlobalScopeSubsystem")) != nullptr);
    CHECK(lRegistries.ObjectClasses().FindByName(OpaaxStringID("class GlobalScopeSubsystem")) == nullptr);
}

TEST_CASE("ComponentRoute: an explicit name overrides the derived one")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    ComponentRegistry& lRegistry = lRegistries.Components();

    // Pinning the name is how a game keeps saved maps loadable across a C++ rename.
    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>(OpaaxStringID("Ammo")));

    CHECK(lRegistry.FindByName(OpaaxStringID("Ammo")) != nullptr);
    CHECK(lRegistry.FindByName(OpaaxStringID("AmmoComponent")) == nullptr);
}

TEST_CASE("ComponentRoute: Count records refusals too")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    ComponentRegistry& lRegistry = lRegistries.Components();

    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());
    CHECK_FALSE(lRegistrar.Components().Register<TestGame::AmmoComponent>()); // duplicate type

    CHECK(lRegistrar.Components().Count() == 2u); // asked twice
    CHECK(lRegistry.Count() == 1u);               // accepted once
}

TEST_CASE("ObjectClassRoute: an unbound route refuses rather than dropping silently")
{
    ModuleRegistrar lRegistrar; // deliberately NOT bound

    CHECK_FALSE(lRegistrar.ObjectClasses().Register<TestGame::PatrolSubsystem>());

    // Asked once, accepted never — the count is what makes a dropped registration visible.
    CHECK(lRegistrar.ObjectClasses().Count() == 1u);
}

TEST_CASE("ObjectClassRoute: a module's subsystem class reaches the engine registry, found as a WorldSubsystem")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    REQUIRE(lRegistrar.ObjectClasses().Register<TestGame::PatrolSubsystem>());

    // Named after the leaf type, and found where every world looks for its subsystems.
    const ObjectClass* lClass = lRegistries.ObjectClasses().FindByName(OpaaxStringID(OpaaxString("PatrolSubsystem")));
    REQUIRE(lClass != nullptr);

    const TDynArray<const ObjectClass*> lFound =
        CollectSubsystemClasses(lRegistries.ObjectClasses(), WorldSubsystem::StaticClass());
    REQUIRE(lFound.size() == 1u);
    CHECK(lFound[0] == lClass);
}

TEST_CASE("ResourceFormatRoute: an unbound route refuses rather than dropping silently")
{
    ModuleRegistrar lRegistrar; // deliberately NOT bound

    CHECK_FALSE(lRegistrar.Resources().Register<TestGame::PickupResource>());

    CHECK(lRegistrar.Resources().Count() == 1u);
}

TEST_CASE("ResourceFormatRoute: a bound route forwards into the engine registry, name derived")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    REQUIRE(lRegistrar.Resources().Register<TestGame::PickupResource>());

    CHECK(lRegistries.Resources().Count() == 1u);

    // The extension the TYPE declared is what the table answers for — the game module said it once
    // and no engine file mentions ".pickup" anywhere.
    const ResourceFormatEntry* const lEntry = lRegistries.Resources().FindByExtension(NormalizeExtension(".pickup"));
    REQUIRE(lEntry != nullptr);
    CHECK(lEntry->Name == OpaaxStringID(OpaaxString("PickupResource")));
}

// =============================================================================
// The dogfood — a module component through the full snapshot core
// =============================================================================
TEST_CASE("Module components round-trip through capture -> instantiate, GUIDs preserved")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    ComponentRegistry& lRegistry = lRegistries.Components();

    // Exactly what SandboxModule::OnRegister does — two types the engine has never heard of.
    REQUIRE(lRegistrar.Components().Register<TestGame::AmmoComponent>());
    REQUIRE(lRegistrar.Components().Register<TestGame::ShieldComponent>());

    const MapId lMap = MapId("Level01");

    World  lWorld("ModuleDogfood");
    Entity lShip = lWorld.CreateEntity("Ship", lMap);
    lShip.Add<TestGame::AmmoComponent>(TestGame::AmmoComponent{250});
    lShip.Add<TestGame::ShieldComponent>(TestGame::ShieldComponent{0.75f});

    const Guid lGuid = lShip.GetGuid();

    const MapData lCaptured = MapSerializer::CaptureMap(lWorld, lRegistry, lMap);
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
