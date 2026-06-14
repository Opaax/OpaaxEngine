// Suite: ComponentEntry<T> Save/Load round-trip — the per-type serialization machinery
// the ComponentRegistry stores for every component.
//
// We instantiate ComponentEntry<T> LOCALLY rather than going through the process-global
// ComponentRegistry static: a static data member of a template can be duplicated across
// the DLL boundary, and the engine populates that registry at startup (not run here). The
// local entry exercises the exact Has/Add/Save/Load contract on a headless World.
#include <doctest.h>

#include "ECS/ComponentRegistry.h"
#include "ECS/Components/TransformComponent.h"
#include "World/World.h"

using namespace Opaax;
using Opaax::ECS::TransformComponent;

TEST_CASE("ComponentEntry: Save -> Load round-trips a TransformComponent across entities")
{
    World lWorld;
    const EntityID lSrc = lWorld.CreateEntity("Src");
    auto& lTr   = lWorld.AddComponent<TransformComponent>(lSrc);
    lTr.Position = { 1.5f, 2.5f };
    lTr.Scale    = { 3.f, 4.f };
    lTr.Rotation = 0.75f;
    lTr.ZOrder   = 6.f;

    const ComponentEntry<TransformComponent> lEntry("TransformComponent", true);
    REQUIRE(lEntry.Has(lWorld, lSrc));

    const nlohmann::json lSaved = lEntry.Save(lWorld, lSrc);

    // Load onto a fresh entity — Load adds the component if absent.
    const EntityID lDst = lWorld.CreateEntity("Dst");
    CHECK_FALSE(lEntry.Has(lWorld, lDst));
    lEntry.Load(lWorld, lDst, lSaved);
    REQUIRE(lEntry.Has(lWorld, lDst));

    const auto* lOut = lWorld.GetComponent<TransformComponent>(lDst);
    REQUIRE(lOut != nullptr);
    CHECK(lOut->Position.x == doctest::Approx(1.5f));
    CHECK(lOut->Position.y == doctest::Approx(2.5f));
    CHECK(lOut->Scale.x    == doctest::Approx(3.f));
    CHECK(lOut->Scale.y    == doctest::Approx(4.f));
    CHECK(lOut->Rotation   == doctest::Approx(0.75f));
    CHECK(lOut->ZOrder     == doctest::Approx(6.f));
}

TEST_CASE("ComponentEntry: name/type-id reflect T, and Add is present-safe")
{
    World lWorld;
    const EntityID lE = lWorld.CreateEntity("E");

    const ComponentEntry<TransformComponent> lEntry("TransformComponent", true);
    CHECK(lEntry.GetName() == OPAAX_ID("TransformComponent"));
    CHECK(lEntry.GetTypeId() == entt::type_hash<TransformComponent>::value());

    CHECK_FALSE(lEntry.Has(lWorld, lE));
    lEntry.Add(lWorld, lE);
    CHECK(lEntry.Has(lWorld, lE));
    lEntry.Add(lWorld, lE); // idempotent — no double-emplace assert
    CHECK(lEntry.Has(lWorld, lE));
}
