// Suite: world-subsystem type identity for a subsystem the engine DLL never sees.
//
// WHY THIS EXISTS (ARCHITECTURE.md I2, lessons L21/L22 — M4 S1).
//   M4's whole point is that a GAME MODULE registers world subsystems: `Sandbox/Module` is a
//   static lib, its subsystem types are compiled into the exe, and the engine DLL never names
//   them. `GetSubsystem<T>()` resolves by comparing a virtual `GetTypeID()` against a
//   caller-side `T::StaticTypeID()` (Subsystem.h:220), and `OPAAX_SUBSYSTEM_TYPE` keys that on
//   the address of a function-local static.
//
//   I2 currently carries a CAVEAT saying a subsystem that is not dll-exported "would get a
//   per-module copy". If that were true of the M4 shape, `GetSubsystem<T>()` would return
//   nullptr for a module's own subsystem and the registry S3 builds on it would be worthless.
//
//   The caveat had never been tested. Before this file, EVERY live user of
//   OPAAX_SUBSYSTEM_TYPE was an OPAAX_API engine subsystem (AudioManager, EngineEventBus,
//   InputManager, RendererManager, ResourceManager, WorldManager) — the *exported* case. The
//   "S9 proof" cited in I2 was a one-off runtime observation of that same exported case, so
//   nothing had ever exercised the non-exported one. L21: prove the premise before designing
//   on it; L22: a contract's claim is a premise, not a finding.
//
//   Expected answer, and the reason I2's caveat is too broad: a game type is compiled into
//   ONE module, so there is no second copy to disagree with. "Not exported" only bites a type
//   that two modules both instantiate (a header-only template static). These cases pin that
//   reasoning so a future change which breaks it — exporting a world subsystem, or moving
//   resolution into the DLL — goes red instead of silently returning null.
//
// THE INSTRUMENT (L21: it must not share a failure mode with the thing it measures).
//   OpaaxTests links the engine IMPORT LIB exactly like Game.exe, so OPAAX_API is dllimport
//   here and `WorldSubsystemMgr` is genuinely the DLL's type. The probes are defined in
//   WorldSubsystemProbes.h and used from TWO TUs (this one and WorldSubsystemIdentityTU2.cpp)
//   with EXTERNAL linkage, because a single-TU test could not tell "one tag per type" apart
//   from "one tag per TU" — and an anonymous namespace would have guaranteed the wrong answer.
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
    // whichever one happened to be registered first (the L4 collision class).
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
// The behavioural statement — how the failure would actually bite in M4
// =============================================================================
TEST_CASE("world subsystem identity: an instance registered in another TU resolves through the DLL's manager")
{
    // WorldSubsystemMgr is the DLL's type (dllimport here) — the same one World owns as
    // m_Subsystems. In M4 the registration happens from the module's factory and the lookup
    // from wherever game code asks, so the two halves are deliberately split across TUs.
    WorldSubsystemMgr lManager;

    RegisterProbeFromOtherTU(lManager);
    lManager.StartupAll();

    // Instantiated HERE, against an instance whose type this TU shares only through a header.
    ProbeWorldSubsystem* lResolvedHere = lManager.GetSubsystem<ProbeWorldSubsystem>();

    // Null is the exact failure I2's caveat predicts. It cannot be an "empty manager"
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
