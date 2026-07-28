// Suite: entt component-type identity across the DLL/exe boundary.
//
// WHY THIS EXISTS (ARCHITECTURE.md I2, lesson L21).
//   ComponentRegistry v2 keys every registered component on `entt::type_hash<T>::value()`, and the
//   game module registers ITS components from the exe side while the engine's entt::registry lives
//   in the DLL. If the two sides computed different ids for the same type, a module component would
//   silently fail to round-trip: no crash, no null, just an empty view — the codebase's worst
//   failure class (L18). That premise had to be PROVEN, not recalled, before the registry was built
//   on top of it.
//
//   Static evidence: entt picks `ENTT_PRETTY_FUNCTION = __FUNCSIG__` on MSVC
//   (Vendors/Entt/.../config/config.h), so `type_hash` is a constexpr hash of the type's signature
//   STRING (core/type_info.hpp:106) — exactly the compiler-stable-per-type-string shape I2 mandates,
//   not the per-module incrementing `type_index` counter it falls back to otherwise.
//
//   These cases are the runtime regression gate for that: they would go red if entt's config ever
//   lost ENTT_PRETTY_FUNCTION (a vendor bump, a stray define), which is the one change that would
//   silently re-introduce the hazard.
//
// THE INSTRUMENT (L21: it must not share a failure mode with the thing it measures).
//   `EntityMeta` is emplaced by `World::CreateEntity`, whose body is compiled INTO THE DLL
//   (World.cpp) — so the pool is created under the DLL's id. Everything below reads it from the
//   exe side through header templates instantiated HERE. A disagreement therefore shows up as an
//   absent pool / empty view rather than as anything the test itself computed.
#include <doctest.h>

#include <entt/entt.hpp>

#include "World/Components/DummyComponent.h"
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
    lEntity.Add<DummyComponent>();

    // World::FindByGuid routes through World.cpp (DLL side) to resolve the handle; reading the
    // component back through a differently-instantiated template must find the same storage.
    Entity lFound = lWorld.FindByGuid(lEntity.GetGuid());
    REQUIRE(lFound.IsValid());
    CHECK(lFound.Has<DummyComponent>());
}
