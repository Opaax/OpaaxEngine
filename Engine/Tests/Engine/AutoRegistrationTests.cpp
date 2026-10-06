// Suite: AutoRegistration — types that register themselves with OPAAX_REGISTER_* macros, collected
// from every translation unit (the engine's and this test's), sorted and de-duplicated.
#include <doctest.h>

#include <algorithm>
#include <cstring>

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Renderer/Components/QuadComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Systems/WorldSubsystem.h"

using namespace Opaax;

namespace AutoRegistrationTestTypes
{
    // A game-style component, registered the way a game module does it.
    struct ProbeHealth
    {
        Int32 Current = 10;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ProbeHealth, Current)
        OPAAX_PROPERTIES(ProbeHealth, OPAAX_PROP(Current))
    };

    OPAAX_REGISTER_COMPONENT(ProbeHealth);

    // The same registration twice, as a macro in a header included by two .cpp files would give.
    OPAAX_REGISTER_COMPONENT(ProbeHealth);

    struct ProbeLateSubsystem final : public WorldSubsystemBase
    {
        OPAAX_SUBSYSTEM_TYPE(ProbeLateSubsystem)
        explicit ProbeLateSubsystem(WorldContext&) {}
        bool Startup() override { return true; }
        void Shutdown() override {}
        static bool ShouldCreate(const World&) { return false; }
    };

    struct ProbeEarlySubsystem final : public WorldSubsystemBase
    {
        OPAAX_SUBSYSTEM_TYPE(ProbeEarlySubsystem)
        explicit ProbeEarlySubsystem(WorldContext&) {}
        bool Startup() override { return true; }
        void Shutdown() override {}
        static bool ShouldCreate(const World&) { return false; }
    };

    // Registered in the "wrong" order on purpose: the order value decides, not the source order.
    OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(ProbeLateSubsystem, WorldSubsystemOrder::Debug + 1);
    OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(ProbeEarlySubsystem, WorldSubsystemOrder::Input - 1);
}

using namespace AutoRegistrationTestTypes;

namespace
{
    Int64 IndexOf(const TDynArray<const AutoRegistration*>& InList, const char* InName)
    {
        for (Uint64 lIndex = 0; lIndex < InList.size(); ++lIndex)
        {
            if (std::strcmp(InList[lIndex]->Name, InName) == 0)
            {
                return static_cast<Int64>(lIndex);
            }
        }
        return -1;
    }

    Int64 IndexOfSubsystem(const WorldSubsystemRegistry& InRegistry, const char* InName)
    {
        Int64 lIndex = 0;
        Int64 lFound = -1;
        InRegistry.ForEach([&](const IWorldSubsystemEntry& InEntry)
        {
            if (InEntry.GetName() == OpaaxStringID(InName)) { lFound = lIndex; }
            ++lIndex;
        });
        return lFound;
    }
}

TEST_CASE("AutoRegistration: the list is sorted by kind, then order, then name")
{
    const TDynArray<const AutoRegistration*> lList = CollectAutoRegistrations();
    REQUIRE_FALSE(lList.empty());

    for (Uint64 lIndex = 1; lIndex < lList.size(); ++lIndex)
    {
        const AutoRegistration& lPrevious = *lList[lIndex - 1];
        const AutoRegistration& lCurrent  = *lList[lIndex];

        const bool bOrdered = lPrevious.Kind < lCurrent.Kind
                           || (lPrevious.Kind == lCurrent.Kind && lPrevious.Order < lCurrent.Order)
                           || (lPrevious.Kind == lCurrent.Kind && lPrevious.Order == lCurrent.Order
                               && std::strcmp(lPrevious.Name, lCurrent.Name) <= 0);
        CHECK(bOrdered);
    }
}

TEST_CASE("AutoRegistration: a type registered twice appears once")
{
    const TDynArray<const AutoRegistration*> lList = CollectAutoRegistrations();

    const auto lCount = std::count_if(lList.begin(), lList.end(), [](const AutoRegistration* InEntry)
    {
        return InEntry->Kind == EAutoRegistrationKind::Component && InEntry->Type == TypeIdOf<ProbeHealth>();
    });
    CHECK(lCount == 1);
}

TEST_CASE("AutoRegistration: engine types and this test's types are both collected")
{
    const TDynArray<const AutoRegistration*> lList = CollectAutoRegistrations();

    // Engine natives (registered in the engine's own feature folders).
    CHECK(IndexOf(lList, "Transform") >= 0);
    CHECK(IndexOf(lList, "Quad") >= 0);
    CHECK(IndexOf(lList, "Physics") >= 0);

    // This translation unit's.
    CHECK(IndexOf(lList, "ProbeHealth") >= 0);
    CHECK(IndexOf(lList, "ProbeEarlySubsystem") >= 0);
}

TEST_CASE("AutoRegistration: running it fills fresh registries, the essential component first")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);

    const Uint64 lRun = RunAutoRegistrations(lRegistrar);
    CHECK(lRun == CollectAutoRegistrations().size());

    // The essential Transform is registered first, whatever its name sorts as.
    const IComponentEntry* lFirst = nullptr;
    lRegistries.Components().ForEach([&](const IComponentEntry& InEntry)
    {
        if (lFirst == nullptr) { lFirst = &InEntry; }
    });
    REQUIRE(lFirst != nullptr);
    CHECK(lFirst->GetName() == OpaaxStringID("Transform"));
    CHECK(lFirst->IsEssential());

    // A game type is registered under its type name, like any engine type.
    const IComponentEntry* lHealth = lRegistries.Components().Find<ProbeHealth>();
    REQUIRE(lHealth != nullptr);
    CHECK(lHealth->GetName() == OpaaxStringID("ProbeHealth"));

    // The alias resolves to the component registered under the new name.
    const IComponentEntry* lQuad = lRegistries.Components().Find<QuadComponent>();
    REQUIRE(lQuad != nullptr);
    CHECK(lRegistries.Components().FindByName(OpaaxStringID("Dummy")) == lQuad);
}

TEST_CASE("AutoRegistration: world subsystems tick in order, whatever the registration order")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    const WorldSubsystemRegistry& lSubsystems = lRegistries.WorldSubsystems();

    const Int64 lEarly   = IndexOfSubsystem(lSubsystems, "ProbeEarlySubsystem");
    const Int64 lPhysics = IndexOfSubsystem(lSubsystems, "Physics");
    const Int64 lMover   = IndexOfSubsystem(lSubsystems, "Mover");
    const Int64 lLate    = IndexOfSubsystem(lSubsystems, "ProbeLateSubsystem");

    REQUIRE(lEarly >= 0);
    REQUIRE(lPhysics >= 0);
    REQUIRE(lMover >= 0);
    REQUIRE(lLate >= 0);

    CHECK(lEarly < lPhysics);
    CHECK(lPhysics < lMover);   // the mover reads the step's poses
    CHECK(lMover < lLate);
}

TEST_CASE("AutoRegistration: game instance subsystems are created UI first, then input mapping")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    TDynArray<OpaaxStringID> lNames;
    lRegistries.GameInstanceSubsystems().ForEach([&](const IGameInstanceSubsystemEntry& InEntry)
    {
        lNames.push_back(InEntry.GetName());
    });

    const auto lUI    = std::find(lNames.begin(), lNames.end(), OpaaxStringID("UI"));
    const auto lInput = std::find(lNames.begin(), lNames.end(), OpaaxStringID("InputMapping"));
    REQUIRE(lUI != lNames.end());
    REQUIRE(lInput != lNames.end());
    CHECK(lUI < lInput);
}
