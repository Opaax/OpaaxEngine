#pragma once

// Probe world subsystems shaped EXACTLY like the ones a game module will define in M4:
// derived from WorldSubsystemBase, stamped with OPAAX_SUBSYSTEM_TYPE, and deliberately
// **not** OPAAX_API — the engine DLL never sees these types.
//
// They live in a HEADER included by two different test translation units on purpose. That
// is the closest available proxy for `Sandbox/Module` (a static lib linked into the exe):
// several TUs each emit their own COMDAT for the inline StaticTypeID(), and the exe linker
// is what must fold them into one. See WorldSubsystemIdentityTests.cpp for why that matters.
//
// INSTRUMENT HAZARD (L21) — this namespace MUST have external linkage. An anonymous
// namespace (or a `static` class) would give every TU its own genuinely distinct type, so
// the cross-TU tag comparison would fail for a reason that has nothing to do with the
// mechanism under test, and "fixing" it would hide the real answer.

#include "Core/OpaaxTypes.h"
#include "Core/Systems/Subsystem.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax::WorldSubsystemProbe
{
    // =============================================================================
    // ProbeWorldSubsystem — the subject.
    // =============================================================================
    class ProbeWorldSubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(ProbeWorldSubsystem)

        bool Startup() override  { ++m_StartupCount; return true; }
        void Shutdown() override { ++m_ShutdownCount; }

        Int32 GetStartupCount() const noexcept  { return m_StartupCount; }
        Int32 GetShutdownCount() const noexcept { return m_ShutdownCount; }

    private:
        Int32 m_StartupCount  = 0;
        Int32 m_ShutdownCount = 0;
    };

    // =============================================================================
    // SecondProbeWorldSubsystem — a DIFFERENT type, structurally identical.
    //
    // Structural identity is the point: the two classes have the same bases, the same
    // layout and the same member bodies, so anything that keyed identity on shape rather
    // than on type would conflate them. Only a per-type tag tells them apart.
    // =============================================================================
    class SecondProbeWorldSubsystem : public WorldSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(SecondProbeWorldSubsystem)

        bool Startup() override  { return true; }
        void Shutdown() override {}
    };

    // =============================================================================
    // The OTHER translation unit's half (WorldSubsystemIdentityTU2.cpp).
    //
    // Each of these evaluates its expression in a TU that is NOT the one running the
    // assertions, which is the whole reason they exist as out-of-line functions.
    // =============================================================================

    /** ProbeWorldSubsystem::StaticTypeID() as computed in the other TU. */
    SubsystemTypeID ProbeTagFromOtherTU();

    /** SecondProbeWorldSubsystem::StaticTypeID() as computed in the other TU. */
    SubsystemTypeID SecondProbeTagFromOtherTU();

    /** RegisterSubsystem<ProbeWorldSubsystem>() instantiated in the other TU. */
    void RegisterProbeFromOtherTU(WorldSubsystemMgr& InManager);

    /** GetSubsystem<ProbeWorldSubsystem>() instantiated in the other TU. */
    ProbeWorldSubsystem* ResolveProbeFromOtherTU(WorldSubsystemMgr& InManager);
}
