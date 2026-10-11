#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES

namespace Opaax
{
    // =============================================================================
    // EPhysicsBackend
    // =============================================================================
    /**
     * Physics implementation. Only Box2D for now.
     */
    enum class EPhysicsBackend : Uint8
    {
        Box2D
    };

    // =============================================================================
    // Backend naming
    // =============================================================================
    /**
     * Name for logs and the value written in the config.
     */
    const char* ToString(EPhysicsBackend InBackend) noexcept;
}

OPAAX_ENUM_VALUES(Opaax::EPhysicsBackend, Box2D)
