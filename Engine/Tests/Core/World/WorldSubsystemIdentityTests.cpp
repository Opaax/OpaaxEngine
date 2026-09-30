// Suite: type identity of a world subsystem the engine DLL never sees (a game module's).
//   GetSubsystem<T>() compares GetTypeID() with T::StaticTypeID() (the address of a function-local
//   static). A game type is compiled into one module, so there is no second copy even though it is
//   not exported. These tests go red if that changes.
//
//   The test exe links the engine import lib like a game. The probes (WorldSubsystemProbes.h) are
//   used from two TUs with external linkage, to tell "one tag per type" from "one tag per TU".
#include <doctest.h>

#include "WorldSubsystemProbes.h"

#include "World/Systems/WorldSubsystem.h"
#include "World/WorldManager.h"

using namespace Opaax;
using namespace Opaax::WorldSubsystemProbe;

// =============================================================================
// The mechanism: one tag per TYPE, not one per translation unit
// =============================================================================
TEST_CASE("world subsystem identity: a non-exported tag is one instance across translation units")
{
    // LHS instantiated here, RHS instantiated in WorldSubsystemIdentityTU2.cpp. Two COMDATs
    // for the same inline function; the exe linker must fold them to one address.
    CHECK(ProbeWorldSubsystem::StaticTypeID() == ProbeTagFromOtherTU());
    CHECK(SecondProbeWorldSubsystem::StaticTypeID() == SecondProbeTagFromOtherTU());
}

// =============================================================================
// Distinctness — what makes a non-null resolve MEAN anything
// =============================================================================
TEST_CASE("world subsystem identity: structurally identical types get distinct tags")
{
    // The two probes have the same base, layout and member bodies. Only per-type identity
    // separates them, so equality here would mean every module subsystem resolved to
    // whichever one happened to be registered first.
    CHECK(ProbeWorldSubsystem::StaticTypeID() != SecondProbeWorldSubsystem::StaticTypeID());
    CHECK(ProbeTagFromOtherTU() != SecondProbeTagFromOtherTU());
}

TEST_CASE("world subsystem identity: an exe-side tag does not collide with a DLL-exported one")
{
    // WorldManager is OPAAX_API and stamped with the same macro, so its tag comes from the
    // DLL's exported inline definition while the probe's comes from the exe. The tag space is
    // shared (SubsystemTypeID is a plain uintptr_t), so this asserts the two modules' statics
    // do not land on one address.
    CHECK(ProbeWorldSubsystem::StaticTypeID() != WorldManager::StaticTypeID());
}

// =============================================================================
// The behavioural statement — how the failure would actually bite
// =============================================================================
TEST_CASE("world subsystem identity: an instance registered in another TU resolves through the DLL's manager")
{
    // WorldSubsystemMgr is the DLL's type (dllimport here) — the same one World owns as
    // m_Subsystems. The registration happens from the module's factory and the lookup
    // from wherever game code asks, so the two halves are deliberately split across TUs.
    WorldSubsystemMgr lManager;

    RegisterProbeFromOtherTU(lManager);
    lManager.StartupAll();

    // Instantiated HERE, against an instance whose type this TU shares only through a header.
    ProbeWorldSubsystem* lResolvedHere = lManager.GetSubsystem<ProbeWorldSubsystem>();

    // Null would be the failure. It cannot be an "empty manager"
    // false negative: the startup count below proves the instance exists and ran.
    REQUIRE(lResolvedHere != nullptr);
    CHECK(lResolvedHere->GetStartupCount() == 1);

    // Both TUs' instantiations must find the SAME object, not merely a non-null one.
    CHECK(lResolvedHere == ResolveProbeFromOtherTU(lManager));

    // The raw comparison GetSubsystem performs, asserted directly: the LHS was written into
    // the vtable by the module that compiled the class, the RHS is computed in this TU.
    CHECK(lResolvedHere->GetTypeID() == ProbeWorldSubsystem::StaticTypeID());
}

TEST_CASE("world subsystem identity: resolution refuses a type that was never registered")
{
    WorldSubsystemMgr lManager;

    RegisterProbeFromOtherTU(lManager);
    lManager.StartupAll();

    // Without this, the case above would pass just as happily under a colliding tag.
    CHECK(lManager.GetSubsystem<SecondProbeWorldSubsystem>() == nullptr);
}

// =============================================================================
// The DLL-owned container drives exe-side overrides
// =============================================================================
TEST_CASE("world subsystem identity: the DLL's manager drives a non-exported subsystem's lifecycle")
{
    WorldSubsystemMgr lManager;

    RegisterProbeFromOtherTU(lManager);
    lManager.StartupAll();

    ProbeWorldSubsystem* lProbe = lManager.GetSubsystem<ProbeWorldSubsystem>();
    REQUIRE(lProbe != nullptr);

    lManager.ShutdownAll();

    // Virtual dispatch from the manager's list into an override the DLL cannot name. This is
    // the half of the boundary that tag identity does not cover: the instance is allocated by
    // the module's factory and stored in a container whose type belongs to the DLL.
    CHECK(lProbe->GetStartupCount() == 1);
    CHECK(lProbe->GetShutdownCount() == 1);
}
