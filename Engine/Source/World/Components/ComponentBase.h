#pragma once

// NOTE: self-contained on purpose — a game module (D9) may include this before any other engine
// header, so it must not rely on include-order luck for OPAAX_API / types.
#include "Core/EngineAPI.h"

namespace Opaax
{
    // =============================================================================
    // ComponentBase — marker base for engine components stored in a World's registry.
    //   Intentionally empty and non-virtual: components are value types (entt stores
    //   them by value, no vtable). Exists only as a common hook for future component
    //   traits shared across every component type. No OPAAX_API: nothing to export from an
    //   empty non-virtual struct (no vtable, no out-of-line members) — the macro was noise.
    // =============================================================================
    struct IComponent
    {
    };

    struct ComponentBase : IComponent
    {
    };
}
