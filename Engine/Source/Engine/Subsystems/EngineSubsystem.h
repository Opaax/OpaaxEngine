#pragma once

#include "Engine/Subsystems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // EngineSubsystem — a subsystem that lives from engine start to engine stop: the engine's own
    //   services (resources, worlds, rendering...), which the engine makes in code, in order.
    // =============================================================================
    class EngineSubsystem : public Subsystem
    {
        OPAAX_ABSTRACT_OBJECT(EngineSubsystem, Subsystem)
    };

    // =============================================================================
    // EngineSubsystemMgr — owns and ticks the engine subsystems (added order; reverse order for
    //   shutdown).
    // =============================================================================
    class EngineSubsystemMgr final : public TSubsystemCollection<EngineSubsystem>
    {
    };
}
