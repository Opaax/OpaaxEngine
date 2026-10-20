// Second translation unit of the world subsystem identity test. No test cases: it instantiates
// StaticClass, Add<T> and GetSubsystem<T> in another TU, so the test can tell "one class per type"
// from "one class per TU".

#include "WorldSubsystemProbes.h"

namespace Opaax::WorldSubsystemProbe
{
    const ObjectClass& ProbeClassFromOtherTU()
    {
        return ProbeWorldSubsystem::StaticClass();
    }

    const ObjectClass& SecondProbeClassFromOtherTU()
    {
        return SecondProbeWorldSubsystem::StaticClass();
    }

    void AddProbeFromOtherTU(WorldSubsystemMgr& InManager)
    {
        InManager.Add<ProbeWorldSubsystem>();
    }

    ProbeWorldSubsystem* ResolveProbeFromOtherTU(WorldSubsystemMgr& InManager)
    {
        return InManager.GetSubsystem<ProbeWorldSubsystem>();
    }
}
