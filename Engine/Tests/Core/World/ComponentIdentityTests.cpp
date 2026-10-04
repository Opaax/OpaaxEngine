// Suite: entt component type ids across the DLL/exe boundary.
//   ComponentRegistry keys components on entt::type_hash<T>::value(); game modules register from
//   the exe while the registry lives in the DLL. Both sides must compute the same id, or a module
//   component would silently not round-trip. On MSVC entt hashes __FUNCSIG__ (ENTT_PRETTY_FUNCTION),
//   which is stable per type; these tests go red if that ever changes.
//
//   EntityMeta is added by World::CreateEntity, compiled in the DLL; the tests read it from the
//   exe side, so a mismatch shows up as an empty view.
#include <doctest.h>

#include <entt/entt.hpp>

#include "World/Components/QuadComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

using namespace Opaax;

// =============================================================================
// The DLL-created pool, found by an exe-computed id
// =============================================================================
TEST_CASE("entt type identity: an exe-side type_hash finds the pool the DLL created")
{
    World lWorld("IdentityProbe");

    // DLL-side: World::CreateEntity (World.cpp) emplaces EntityMeta, creating its pool under
    // whatever id the DLL's translation unit computed.
    lWorld.CreateEntity("A");
    lWorld.CreateEntity("B");

    // Exe-side: this instantiation of type_hash<EntityMeta> happens in THIS translation unit.
    const entt::id_type lExeSideId = entt::type_hash<EntityMeta>::value();

    const auto* lPool = lWorld.GetRegistry().storage(lExeSideId);

    // Null here == the two sides disagree. That is the whole point of the case: the pool
    // demonstrably exists (two entities were just created), so absence can only mean a
    // mismatched id.
    REQUIRE(lPool != nullptr);
    CHECK(lPool->size() == 2u);
}

// =============================================================================
// The behavioural statement — this is how the mismatch would actually bite
// =============================================================================
TEST_CASE("entt type identity: an exe-side view sees components emplaced by the DLL")
{
    World lWorld("IdentityProbe");

    lWorld.CreateEntity("A");
    lWorld.CreateEntity("B");
    lWorld.CreateEntity("C");

    // World::Each<T> is a header template, so this view is built exe-side. It is also the exact
    // call MapSerializer::Capture uses as its all-entities view — if this can go wrong, capture
    // silently returns an empty map.
    int lSeen = 0;
    lWorld.Each<EntityMeta>([&](const EntityMeta&) { ++lSeen; });

    CHECK(lSeen == 3);
}

// =============================================================================
// Round trip through the boundary in the other direction
// =============================================================================
TEST_CASE("entt type identity: a component added exe-side is visible to a DLL-side lookup")
{
    World  lWorld("IdentityProbe");
    Entity lEntity = lWorld.CreateEntity("Subject");

    // Entity::Add<T> is a header template -> the emplace, and the pool it may create, happen
    // under the EXE's id.
    lEntity.Add<QuadComponent>();

    // World::FindByGuid routes through World.cpp (DLL side) to resolve the handle; reading the
    // component back through a differently-instantiated template must find the same storage.
    Entity lFound = lWorld.FindByGuid(lEntity.GetGuid());
    REQUIRE(lFound.IsValid());
    CHECK(lFound.Has<QuadComponent>());
}
