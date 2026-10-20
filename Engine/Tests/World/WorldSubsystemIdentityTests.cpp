// Suite: class identity of a world subsystem the engine never sees (a game module's).
//   GetSubsystem<T>() asks each subsystem IsA(T::StaticClass()), and StaticClass() is a
//   function-local static in an inline function, so one class per type for the whole program.
//   These tests go red if that changes.
//
//   The test exe links the engine library like a game. The probes (WorldSubsystemProbes.h) are
//   used from two TUs with external linkage, to tell "one class per type" from "one per TU".
#include <doctest.h>

#include "WorldSubsystemProbes.h"

#include "World/Systems/WorldSubsystem.h"
#include "World/WorldManager.h"

using namespace Opaax;
using namespace Opaax::WorldSubsystemProbe;

// =============================================================================
// The mechanism: one class per TYPE, not one per translation unit
// =============================================================================
TEST_CASE("world subsystem identity: a class is one instance across translation units")
{
    // LHS instantiated here, RHS instantiated in WorldSubsystemIdentityTU2.cpp. Two COMDATs for the
    // same inline function; the exe linker must fold them to one address.
    CHECK(&ProbeWorldSubsystem::StaticClass() == &ProbeClassFromOtherTU());
    CHECK(&SecondProbeWorldSubsystem::StaticClass() == &SecondProbeClassFromOtherTU());
}

// =============================================================================
// Distinctness — what makes a non-null resolve MEAN anything
// =============================================================================
TEST_CASE("world subsystem identity: structurally identical types get distinct classes")
{
    // The two probes have the same base, layout and member bodies. Only per-type identity separates
    // them, so equality here would mean every module subsystem resolved to whichever one was made
    // first.
    CHECK(&ProbeWorldSubsystem::StaticClass() != &SecondProbeWorldSubsystem::StaticClass());
    CHECK_FALSE(ProbeWorldSubsystem::StaticClass().IsChildOf(SecondProbeWorldSubsystem::StaticClass()));
    CHECK(ProbeWorldSubsystem::StaticClass().GetTypeId() != SecondProbeWorldSubsystem::StaticClass().GetTypeId());
}

TEST_CASE("world subsystem identity: a game-side class does not collide with an engine one")
{
    // WorldManager's class comes from the engine's inline definition, the probe's from this test.
    CHECK(&ProbeWorldSubsystem::StaticClass() != &WorldManager::StaticClass());
    CHECK(ProbeWorldSubsystem::StaticClass().GetTypeId() != WorldManager::StaticClass().GetTypeId());
}

// =============================================================================
// The behavioural statement — how the failure would actually bite
// =============================================================================
TEST_CASE("world subsystem identity: an instance added in another TU resolves through the engine's manager")
{
    // WorldSubsystemMgr is the engine's type — the one World owns. The adding happens in one TU and
    // the lookup wherever game code asks, so the two halves are deliberately split across TUs.
    WorldSubsystemMgr lManager;

    AddProbeFromOtherTU(lManager);
    lManager.StartupAll();

    // Instantiated HERE, against an instance whose type this TU shares only through a header.
    ProbeWorldSubsystem* lResolvedHere = lManager.GetSubsystem<ProbeWorldSubsystem>();

    // Null would be the failure. It cannot be an "empty manager" false negative: the startup count
    // below proves the instance exists and ran.
    REQUIRE(lResolvedHere != nullptr);
    CHECK(lResolvedHere->GetStartupCount() == 1);

    // Both TUs' instantiations must find the SAME object, not merely a non-null one.
    CHECK(lResolvedHere == ResolveProbeFromOtherTU(lManager));

    // The class the object reports was written into the vtable by the TU that made it.
    CHECK(&lResolvedHere->GetClass() == &ProbeWorldSubsystem::StaticClass());
}

TEST_CASE("world subsystem identity: resolution refuses a type that was never added")
{
    WorldSubsystemMgr lManager;

    AddProbeFromOtherTU(lManager);
    lManager.StartupAll();

    // Without this, the case above would pass just as happily under a colliding class.
    CHECK(lManager.GetSubsystem<SecondProbeWorldSubsystem>() == nullptr);
}

// =============================================================================
// The engine-owned container drives game-side overrides
// =============================================================================
TEST_CASE("world subsystem identity: the engine's manager drives a game subsystem's lifecycle")
{
    WorldSubsystemMgr lManager;

    AddProbeFromOtherTU(lManager);
    lManager.StartupAll();

    ProbeWorldSubsystem* lProbe = lManager.GetSubsystem<ProbeWorldSubsystem>();
    REQUIRE(lProbe != nullptr);

    lManager.ShutdownAll();

    // Virtual dispatch from the manager's list into an override the engine cannot name: the
    // instance is made in the module's TU and stored in a container whose type is the engine's.
    CHECK(lProbe->GetStartupCount() == 1);
    CHECK(lProbe->GetShutdownCount() == 1);
}
