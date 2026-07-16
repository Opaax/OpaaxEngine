#pragma once

namespace Opaax
{
    // =============================================================================
    // ComponentBase — marker base for engine components stored in a World's registry.
    //   Intentionally empty and non-virtual: components are value types (entt stores
    //   them by value, no vtable). Exists only as a common hook for future component
    //   traits shared across every component type.
    // =============================================================================
    struct ComponentBase
    {
    };
}
