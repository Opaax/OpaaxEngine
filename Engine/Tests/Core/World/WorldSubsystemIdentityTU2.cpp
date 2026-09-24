// The second translation unit of the world-subsystem identity probe.
//
// It contains no test cases. Its only job is to instantiate the identity machinery
// (StaticTypeID, RegisterSubsystem<T>, GetSubsystem<T>) somewhere OTHER than the TU that
// asserts on the results — a same-TU comparison could not distinguish "one tag per type"
// from "one tag per translation unit", which is precisely the I2 question M4 rests on.

#include "WorldSubsystemProbes.h"

namespace Opaax::WorldSubsystemProbe
{
    SubsystemTypeID ProbeTagFromOtherTU()
    {
        return ProbeWorldSubsystem::StaticTypeID();
    }

    SubsystemTypeID SecondProbeTagFromOtherTU()
    {
        return SecondProbeWorldSubsystem::StaticTypeID();
    }

    void RegisterProbeFromOtherTU(WorldSubsystemMgr& InManager)
    {
        InManager.RegisterSubsystem<ProbeWorldSubsystem>();
    }

    ProbeWorldSubsystem* ResolveProbeFromOtherTU(WorldSubsystemMgr& InManager)
    {
        return InManager.GetSubsystem<ProbeWorldSubsystem>();
    }
}
