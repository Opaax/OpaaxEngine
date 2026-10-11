// Suite: entt component type ids across translation units.
//   entt finds a component's pool by entt::type_hash<T>::value(); game modules add components from
//   their own TUs while the world is compiled in the engine library. Both sides must compute the
//   same id, or a module component would silently be invisible. entt hashes the compiler's function
//   signature, which is stable per type; these tests go red if that ever changes. (ComponentRegistry
//   itself keys on the engine's TypeIdOf<T>.)
//
//   EntityMeta is added by World::CreateEntity, compiled in the engine; the tests read it from this
//   TU, so a mismatch shows up as an empty view.
#include <doctest.h>

#include <entt/entt.hpp>

#include "Renderer/Components/QuadComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

using namespace Opaax;

// =============================================================================
// The engine-created pool, found by an id computed here
// =============================================================================
TEST_CASE("entt type identity: a type_hash computed in a game TU finds the pool the engine created")
{
    World lWorld("IdentityProbe");

    // Engine side: World::CreateEntity (World.cpp) emplaces EntityMeta, creating its pool under
    // whatever id World.cpp computed.
    lWorld.CreateEntity("A");
    lWorld.CreateEntity("B");

    // Game side: this instantiation of type_hash<EntityMeta> happens in THIS translation unit.
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
TEST_CASE("entt type identity: a view in a game TU sees components emplaced by the engine")
{
    World lWorld("IdentityProbe");

    lWorld.CreateEntity("A");
    lWorld.CreateEntity("B");
    lWorld.CreateEntity("C");

    // World::Each<T> is a header template, so this view is built in this TU. It is also the exact
    // call MapSerializer::Capture uses as its all-entities view — if this can go wrong, capture
    // silently returns an empty map.
    int lSeen = 0;
    lWorld.Each<EntityMeta>([&](const EntityMeta&) { ++lSeen; });

    CHECK(lSeen == 3);
}

// =============================================================================
// Round trip through the boundary in the other direction
// =============================================================================
TEST_CASE("entt type identity: a component added in a game TU is visible to an engine lookup")
{
    World  lWorld("IdentityProbe");
    Entity lEntity = lWorld.CreateEntity("Subject");

    // Entity::Add<T> is a header template -> the emplace, and the pool it may create, happen
    // under THIS TU's id.
    lEntity.Add<QuadComponent>();

    // World::FindByGuid routes through World.cpp (engine side) to resolve the handle; reading the
    // component back through a differently-instantiated template must find the same storage.
    Entity lFound = lWorld.FindByGuid(lEntity.GetGuid());
    REQUIRE(lFound.IsValid());
    CHECK(lFound.Has<QuadComponent>());
}
