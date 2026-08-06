// Suite: WorldManager::CloneWorld — the PIE clone (M4 S4).
//
// WHY THIS EXISTS.
//   Play clones the edit world into an isolated Play world that Stop throws away. The whole
//   restore-is-free property rests on two claims this suite pins: the clone is FAITHFUL (same
//   entities, same Guids — WM3, or every inter-entity reference silently retargets), and the two
//   worlds are INDEPENDENT (entt handles are per-registry, so a clone must not be able to reach
//   back into the world it copied).
//
//   CloneWorld is composition — Capture -> CreateWorld -> Instantiate — so the risk is not in the
//   halves (M3 covers those) but in what the composition promises on top: an UNFILTERED capture,
//   a mode that comes from the CALLER, and a source left untouched.
//
// WHAT THIS DOES NOT COVER.
//   The clone's SUBSYSTEM set. Creating subsystems needs a started engine (WorldManager::Startup
//   resolves ResourceManager/EngineEventBus/DebugDraw from the locator), which a test has no way
//   to stand up — the same boundary WorldSubsystemRegistryTests.cpp documents. What is asserted
//   here is the mechanism that decides it: the clone is built with ITS OWN mode. That mode drives
//   the same filtered creation path every world takes, whose per-mode outcome is pinned by the
//   registry suite and observed for real in the hosts' boot log (L22).
#include <doctest.h>

#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    // A component defined exe-side, as a game module's would be.
    struct LoadoutComponent
    {
        int Ammo = 0;

        bool operator==(const LoadoutComponent& InOther) const { return Ammo == InOther.Ammo; }
    };

    inline void to_json(nlohmann::json& InJson, const LoadoutComponent& InValue)
    {
        InJson = nlohmann::json{{"Ammo", InValue.Ammo}};
    }

    inline void from_json(const nlohmann::json& InJson, LoadoutComponent& InValue)
    {
        InJson.at("Ammo").get_to(InValue.Ammo);
    }

    // Deliberately NEVER registered — the "what does a clone NOT carry" case.
    struct UnregisteredComponent
    {
        int Value = 0;
    };

    // Registration must happen BEFORE the first CreateWorld: that call seals the registries
    // (BO4), and a type registered after it would be missing from the world that already exists.
    void FillRegistries(EngineRegistries& InRegistries)
    {
        REQUIRE(InRegistries.Components().Register<DummyComponent>("Dummy"));
        REQUIRE(InRegistries.Components().Register<LoadoutComponent>("Loadout"));
    }
}

// =============================================================================
// Fidelity — the gate
// =============================================================================
TEST_CASE("world clone: entities arrive with the same Guids, names and owner map")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    const MapId lMap = MapId("Level01");

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);

    Entity lHero = lSource->CreateEntity("Hero", lMap);
    lHero.Add<LoadoutComponent>(LoadoutComponent{42});
    lHero.Add<DummyComponent>();
    lHero.Get<DummyComponent>().Position = Vector2F{12.f, -3.f};

    Entity lCrate = lSource->CreateEntity("Crate", lMap);
    lCrate.Add<DummyComponent>();

    const Guid lHeroGuid  = lHero.GetGuid();
    const Guid lCrateGuid = lCrate.GetGuid();

    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);

    CHECK(lClone->GetEntityCount() == lSource->GetEntityCount());

    // Identity survived — the SAME Guids resolve in the clone (WM3).
    Entity lClonedHero = lClone->FindByGuid(lHeroGuid);
    REQUIRE(lClonedHero.IsValid());
    CHECK(lClonedHero.Get<EntityMeta>().Name == "Hero");
    CHECK(lClonedHero.Get<EntityMeta>().OwnerMap == lMap);

    REQUIRE(lClonedHero.Has<LoadoutComponent>());
    CHECK(lClonedHero.Get<LoadoutComponent>() == LoadoutComponent{42});
    REQUIRE(lClonedHero.Has<DummyComponent>());
    CHECK(lClonedHero.Get<DummyComponent>().Position.x == doctest::Approx(12.f));
    CHECK(lClonedHero.Get<DummyComponent>().Position.y == doctest::Approx(-3.f));

    Entity lClonedCrate = lClone->FindByGuid(lCrateGuid);
    REQUIRE(lClonedCrate.IsValid());
    CHECK(lClonedCrate.Has<DummyComponent>());
    CHECK_FALSE(lClonedCrate.Has<LoadoutComponent>()); // it never had one

    // The clone is a world in its own right: its own identity, and the manager owns both.
    CHECK(lClone->GetId() != lSource->GetId());
    CHECK(lClone->GetName() == lSource->GetName()); // same level; the MODE is what tells them apart
    CHECK(lWorlds.GetWorldCount() == 2u);
}

TEST_CASE("world clone: the capture is UNFILTERED, so runtime-spawned entities come along")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Play);
    REQUIRE(lSource != nullptr);

    Entity lAuthored = lSource->CreateEntity("Authored", MapId("Level01"));
    lAuthored.Add<LoadoutComponent>(LoadoutComponent{1});

    // No owner map = runtime-spawned (a bullet). A SAVE would skip it; a clone must not, or the
    // Play world starts out already diverged from the world it copied (WM2).
    Entity lBullet = lSource->CreateEntity("Bullet");
    lBullet.Add<LoadoutComponent>(LoadoutComponent{2});

    const Guid lBulletGuid = lBullet.GetGuid();

    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);

    CHECK(lClone->GetEntityCount() == 2u);

    Entity lClonedBullet = lClone->FindByGuid(lBulletGuid);
    REQUIRE(lClonedBullet.IsValid());
    CHECK(lClonedBullet.Get<LoadoutComponent>() == LoadoutComponent{2});
    CHECK_FALSE(lClonedBullet.Get<EntityMeta>().OwnerMap.IsValid()); // still runtime-spawned
}

TEST_CASE("world clone: a component type the registry does not know does not survive")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries); // UnregisteredComponent deliberately absent

    WorldManager lWorlds(&lRegistries);

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);

    Entity lEntity = lSource->CreateEntity("Subject", MapId("Level01"));
    lEntity.Add<LoadoutComponent>(LoadoutComponent{9});
    lEntity.Add<UnregisteredComponent>(UnregisteredComponent{7});

    const Guid lGuid = lEntity.GetGuid();

    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);

    // A clone is a snapshot round trip, so it carries exactly what the registry knows (WM6).
    // An unregistered type has no stable name to be written under — it is not a clone bug.
    Entity lCloned = lClone->FindByGuid(lGuid);
    REQUIRE(lCloned.IsValid());
    CHECK(lCloned.Has<LoadoutComponent>());
    CHECK_FALSE(lCloned.Has<UnregisteredComponent>());
}

// =============================================================================
// Mode — chosen by the caller, never inherited
// =============================================================================
TEST_CASE("world clone: the clone runs in the mode ASKED FOR, and the source keeps its own")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    World* lEditWorld = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lEditWorld != nullptr);

    // The PIE direction: Edit -> Play. The mode is what ShouldCreate reads, so this is the line
    // that makes the clone take the PLAY subsystem set and leave the edit overlays behind (WS2).
    World* lPlayClone = lWorlds.CloneWorld(*lEditWorld, EWorldMode::Play);
    REQUIRE(lPlayClone != nullptr);

    CHECK(lPlayClone->GetMode() == EWorldMode::Play);
    CHECK(lEditWorld->GetMode() == EWorldMode::Edit); // a mode is fixed for a world's whole life

    // And it is genuinely a parameter, not a flip: cloning back the other way is symmetric.
    World* lEditClone = lWorlds.CloneWorld(*lPlayClone, EWorldMode::Edit);
    REQUIRE(lEditClone != nullptr);
    CHECK(lEditClone->GetMode() == EWorldMode::Edit);

    // Cloning into the SAME mode is legal too (a checkpoint, not a mode switch).
    World* lSameMode = lWorlds.CloneWorld(*lEditWorld, EWorldMode::Edit);
    REQUIRE(lSameMode != nullptr);
    CHECK(lSameMode->GetMode() == EWorldMode::Edit);
}

// =============================================================================
// Independence — what makes Stop free
// =============================================================================
TEST_CASE("world clone: the two worlds are independent in BOTH directions")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);

    Entity lSubject = lSource->CreateEntity("Subject", MapId("Level01"));
    lSubject.Add<LoadoutComponent>(LoadoutComponent{10});

    Entity lDoomed = lSource->CreateEntity("Doomed", MapId("Level01"));
    lDoomed.Add<LoadoutComponent>(LoadoutComponent{99});

    const Guid lSubjectGuid = lSubject.GetGuid();
    const Guid lDoomedGuid  = lDoomed.GetGuid();

    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);

    // Play mutates: edit a value, destroy an entity, spawn a new one.
    lClone->FindByGuid(lSubjectGuid).Get<LoadoutComponent>().Ammo = 0;
    lClone->DestroyEntity(lClone->FindByGuid(lDoomedGuid));
    lClone->CreateEntity("SpawnedDuringPlay");

    // None of it reached the edit world — which is the ENTIRE restore mechanism: Stop does not
    // undo anything, because nothing was ever done to the source.
    CHECK(lSource->FindByGuid(lSubjectGuid).Get<LoadoutComponent>().Ammo == 10);
    CHECK(lSource->FindByGuid(lDoomedGuid).IsValid());
    CHECK(lSource->GetEntityCount() == 2u);

    // And the other direction: the source is not a live feed into the clone.
    lSource->FindByGuid(lSubjectGuid).Get<LoadoutComponent>().Ammo = 555;
    CHECK(lClone->FindByGuid(lSubjectGuid).Get<LoadoutComponent>().Ammo == 0);
}

TEST_CASE("world clone: destroying the clone leaves the source whole — the Stop path")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);

    Entity lEntity = lSource->CreateEntity("Survivor", MapId("Level01"));
    lEntity.Add<LoadoutComponent>(LoadoutComponent{5});

    const Guid lGuid = lEntity.GetGuid();

    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);
    REQUIRE(lWorlds.GetWorldCount() == 2u);

    // Stop: re-activate the edit world, throw the clone away. Runtime state dies with it.
    REQUIRE(lWorlds.SetActiveWorld(lSource));
    lWorlds.DestroyWorld(lClone);

    CHECK(lWorlds.GetWorldCount() == 1u);
    CHECK(lWorlds.GetActiveWorld() == lSource);

    Entity lSurvivor = lSource->FindByGuid(lGuid);
    REQUIRE(lSurvivor.IsValid());
    CHECK(lSurvivor.Get<LoadoutComponent>() == LoadoutComponent{5});
    CHECK(lSource->GetEntityCount() == 1u);
}

// =============================================================================
// Refusals, asserted rather than worked around
// =============================================================================
TEST_CASE("world clone: a manager with no registries REFUSES to clone")
{
    // A bare manager can still CreateWorld — a world with no subsystems is a valid outcome there.
    // Cloning is different: with no ComponentRegistry the capture is empty by construction, so the
    // "clone" would be an empty world masquerading as a copy. Refuse loudly instead (L22).
    WorldManager lWorlds;

    World* lSource = lWorlds.CreateWorld("Bare", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);
    lSource->CreateEntity("Ghost", MapId("Level01"));

    CHECK(lWorlds.CloneWorld(*lSource, EWorldMode::Play) == nullptr);
    CHECK(lWorlds.GetWorldCount() == 1u); // and nothing was created on the way out
}

TEST_CASE("world clone: cloning an EMPTY world yields an empty world, not a failure")
{
    EngineRegistries lRegistries;
    FillRegistries(lRegistries);

    WorldManager lWorlds(&lRegistries);

    World* lSource = lWorlds.CreateWorld("Main", EWorldMode::Edit);
    REQUIRE(lSource != nullptr);

    // Pressing Play before authoring anything is not an error.
    World* lClone = lWorlds.CloneWorld(*lSource, EWorldMode::Play);
    REQUIRE(lClone != nullptr);

    CHECK(lClone->GetEntityCount() == 0u);
    CHECK(lClone->GetMode() == EWorldMode::Play);
    CHECK(lWorlds.GetWorldCount() == 2u);
}
