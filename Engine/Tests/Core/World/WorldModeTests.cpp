// Suite: EWorldMode + the WorldSpec seam (M4 S2).
//
// WHY THIS EXISTS.
//   S2 split world creation into POLICY (the host answers GetStartupWorldSpec) and MECHANISM
//   (IEngine::FinishStartup creates it). Before it, OpaaxApplication::CreateStartupWorld reached
//   through the engine to drive WorldManager itself — the same smell MR0 removed for registries,
//   and the subject of two user TODOs in the tree.
//
//   The load-bearing property is that a mode CANNOT be changed after construction. PIE-by-clone
//   depends on it: Play runs a clone, so Stop restores the edit world by simply discarding the
//   clone rather than undoing anything. A settable mode would re-introduce exactly the "put the
//   world back after playing" problem cloning exists to avoid. That is enforced by the type (no
//   setter exists), so the cases below pin the reachable half: what a mode IS at creation, and
//   that the query which decides it has no side effects.
//
// WHAT THIS DELIBERATELY DOES NOT COVER.
//   That the runtime host boots Play and the editor boots Edit is a WIRING fact — it depends on
//   which override runs during a real boot, and a test constructing things directly cannot see
//   it (L22: all 162 tests passed while the boot order was broken). Its gate is the ordered boot
//   log of both hosts, checked in the S2 verification.
#include <doctest.h>

#include "Application/WorldSpec.h"
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

    // Two worlds coexisting with different modes is the PIE shape S4/S5 need, and CreateWorld
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
