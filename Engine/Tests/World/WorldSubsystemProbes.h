#pragma once

// Probe world subsystems shaped like a game module's: WorldSubsystemBase, OPAAX_SUBSYSTEM_TYPE.
// In a header included by two test TUs (like a static lib linked into the exe).
// The namespace must have external linkage: an anonymous one would give each TU its own type.

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
