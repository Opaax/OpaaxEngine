#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

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

        // _WITH_DEFAULT so a field added later does not refuse every map already saved — the
        // plain macro reads with at(), which throws on a missing key.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HealthComponent, Current, Max)

        // Two lines, and this component is editable — it had no drawer at all before, so nothing
        // in the Inspector could see it.
        OPAAX_PROPERTIES(HealthComponent,
                         OPAAX_PROP(Current),
                         OPAAX_PROP(Max))
    };
}
