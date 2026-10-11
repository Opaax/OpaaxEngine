// Suite: EWorldMode and WorldSpec.
//   A world's mode cannot change after construction (Play In Editor runs a copy, so Stop just
//   discards it). Tested here: the mode at creation, and that the startup spec query has no side
//   effects. Which mode each host boots is checked by their boot log.
#include <doctest.h>

#include "World/WorldSpec.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

// =============================================================================
// The mode a world is born with
// =============================================================================
TEST_CASE("world mode: a world carries the mode it was constructed with")
{
    World lEditWorld("Edited", EWorldMode::Edit);
    World lPlayWorld("Played", EWorldMode::Play);

    CHECK(lEditWorld.GetMode() == EWorldMode::Edit);
    CHECK(lPlayWorld.GetMode() == EWorldMode::Play);
}

TEST_CASE("world mode: the default is Play, so a bare world is runnable")
{
    // 22 existing call sites construct a World with a name only. They must keep meaning
    // something, and "runnable" is the honest default for a world nobody labelled.
    World lWorld("Unlabelled");

    CHECK(lWorld.GetMode() == EWorldMode::Play);
}

// =============================================================================
// Through the manager — the path FinishStartup takes
// =============================================================================
TEST_CASE("world mode: WorldManager::CreateWorld forwards the mode")
{
    WorldManager lWorlds; // no registries: a bare manager has nothing to seal

    World* lEdit = lWorlds.CreateWorld("EditWorld", EWorldMode::Edit);
    World* lPlay = lWorlds.CreateWorld("PlayWorld", EWorldMode::Play);

    REQUIRE(lEdit != nullptr);
    REQUIRE(lPlay != nullptr);

    CHECK(lEdit->GetMode() == EWorldMode::Edit);
    CHECK(lPlay->GetMode() == EWorldMode::Play);

    // Two worlds coexisting with different modes is the PIE shape needs, and CreateWorld
    // must not have activated either — the caller decides (a clone is created before it is shown).
    CHECK(lWorlds.GetWorldCount() == 2u);
    CHECK(lWorlds.GetActiveWorld() == nullptr);
}

TEST_CASE("world mode: a mode set at creation survives being made active")
{
    WorldManager lWorlds;

    World* lWorld = lWorlds.CreateWorld("EditWorld", EWorldMode::Edit);
    REQUIRE(lWorld != nullptr);
    REQUIRE(lWorlds.SetActiveWorld(lWorld));

    CHECK(lWorlds.GetActiveWorld() == lWorld);
    CHECK(lWorlds.GetActiveWorld()->GetMode() == EWorldMode::Edit);
}

// =============================================================================
// WorldSpec — the seam type itself
// =============================================================================
TEST_CASE("world spec: defaults to Play with no level")
{
    WorldSpec lSpec;

    CHECK(lSpec.LevelPath.IsEmpty());
    CHECK(lSpec.Mode == EWorldMode::Play);
}

TEST_CASE("world spec: the spec's mode is the mode the world is created in")
{
    // What Engine::FinishStartup does with the host's answer, minus the engine. The NAME is not
    // the host's to give — it comes from the level (LevelData::Name), or NullLevel when there
    // is none — so the spec carries only the mode and the path.
    WorldSpec lSpec;
    lSpec.Mode = EWorldMode::Edit;

    WorldManager lWorlds;
    World*       lWorld = lWorlds.CreateWorld(OpaaxString(NULL_LEVEL_WORLD_NAME), lSpec.Mode);

    REQUIRE(lWorld != nullptr);
    CHECK(lWorld->GetName() == OpaaxString("NullLevel"));
    CHECK(lWorld->GetMode() == EWorldMode::Edit);
}

TEST_CASE("world mode: ToString labels both modes")
{
    // Used by the boot log, which is the ONLY instrument that can show a wiring mistake here
    // (see the note at the top of this file), so it must not silently answer the same thing twice.
    CHECK(OpaaxString(ToString(EWorldMode::Edit)) == OpaaxString("Edit"));
    CHECK(OpaaxString(ToString(EWorldMode::Play)) == OpaaxString("Play"));
}
