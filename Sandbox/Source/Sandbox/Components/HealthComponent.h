#pragma once

#include <nlohmann/json.hpp>

namespace Sandbox
{
    // =============================================================================
    // HealthComponent — a GAME component, owned by the Sandbox module and unknown to the
    //   engine. It exists to prove the M3 gate's "including module components" half: this
    //   type is defined and registered in the EXE, while the entt registry it lands in and
    //   the ComponentRegistry that serializes it both live in the engine DLL.
    //
    //   Note what registering it costs: the NLOHMANN macro, and one line in OnRegister.
    //   There is NO base class to derive from — satisfying CComponent is the whole contract.
    //   No engine header lists it; nothing in the engine names it.
    // =============================================================================
    struct HealthComponent
    {
        int Current = 100;
        int Max     = 100;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(HealthComponent, Current, Max)
    };
}
