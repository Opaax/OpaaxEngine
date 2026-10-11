// Second translation unit of the world subsystem identity test. No test cases: it instantiates
// StaticTypeID, RegisterSubsystem<T> and GetSubsystem<T> in another TU, so the test can tell
// "one tag per type" from "one tag per TU".

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
