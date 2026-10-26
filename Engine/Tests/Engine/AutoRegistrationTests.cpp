// Suite: AutoRegistration — types that register themselves with OPAAX_REGISTER_* macros, collected
// from every translation unit (the engine's and this test's), sorted and de-duplicated.
#include <doctest.h>

#include "Core/Algo/Algo.h"
#include "Core/String/CString.h"

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
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

    // Never made (ShouldCreate refuses every world): they only take a place in the tick order.
    class ProbeLateSubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(ProbeLateSubsystem, WorldSubsystem)

        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Debug + 1; }
        bool  ShouldCreate(const World&) const override { return false; }
    };

    class ProbeEarlySubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(ProbeEarlySubsystem, WorldSubsystem)

        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Input - 1; }
        bool  ShouldCreate(const World&) const override { return false; }
    };

    // Registered in the "wrong" order on purpose: the tick order decides, not the source order.
    OPAAX_REGISTER_OBJECT(ProbeLateSubsystem);
    OPAAX_REGISTER_OBJECT(ProbeEarlySubsystem);
}

using namespace AutoRegistrationTestTypes;

namespace
{
    Int64 IndexOf(const TDynArray<const AutoRegistration*>& InList, const char* InName)
    {
        for (Uint64 lIndex = 0; lIndex < InList.size(); ++lIndex)
        {
            if (CString::Equals(InList[lIndex]->Name, InName))
            {
                return static_cast<Int64>(lIndex);
            }
        }
        return -1;
    }

    Int64 IndexOfClass(const TDynArray<const ObjectClass*>& InClasses, const char* InName)
    {
        for (Uint64 lIndex = 0; lIndex < InClasses.size(); ++lIndex)
        {
            if (InClasses[lIndex]->GetName() == OpaaxStringID(InName))
            {
                return static_cast<Int64>(lIndex);
            }
        }
        return -1;
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
                               && CString::Compare(lPrevious.Name, lCurrent.Name) <= 0);
        CHECK(bOrdered);
    }
}

TEST_CASE("AutoRegistration: a type registered twice appears once")
{
    const TDynArray<const AutoRegistration*> lList = CollectAutoRegistrations();

    const auto lCount = Algo::CountIf(lList, [](const AutoRegistration* InEntry)
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
    CHECK(IndexOf(lList, "PhysicsSubsystem") >= 0);

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

    const TDynArray<const ObjectClass*> lSubsystems =
        CollectSubsystemClasses(lRegistries.ObjectClasses(), WorldSubsystem::StaticClass());

    // Names sort the other way (Physics after Mover, the early probe after Physics): the order decides.
    const Int64 lEarly   = IndexOfClass(lSubsystems, "ProbeEarlySubsystem");
    const Int64 lPhysics = IndexOfClass(lSubsystems, "PhysicsSubsystem");
    const Int64 lMover   = IndexOfClass(lSubsystems, "MoverSubsystem");
    const Int64 lLate    = IndexOfClass(lSubsystems, "ProbeLateSubsystem");

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

    const TDynArray<const ObjectClass*> lSubsystems =
        CollectSubsystemClasses(lRegistries.ObjectClasses(), GameInstanceSubsystem::StaticClass());

    const Int64 lBank  = IndexOfClass(lSubsystems, "AudioGlobalBank");
    const Int64 lUI    = IndexOfClass(lSubsystems, "UISubsystem");
    const Int64 lInput = IndexOfClass(lSubsystems, "InputMappingSubsystem");
    REQUIRE(lBank >= 0);
    REQUIRE(lUI >= 0);
    REQUIRE(lInput >= 0);
    CHECK(lBank < lUI);
    CHECK(lUI < lInput);
}
