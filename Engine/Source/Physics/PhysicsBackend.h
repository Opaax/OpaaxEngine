#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES — the backend's own value list

namespace Opaax
{
    // =============================================================================
    // EPhysicsBackend
    // =============================================================================
    /**
     * @enum EPhysicsBackend
     *
     * Which implementation backs IPhysicsWorld. Box2D is the only one today; a custom solver
     * or a second library are the intended future entries. Selection happens once, from
     * config, when a Play world builds its physics subsystem.
     *
     * Its own header rather than a member of PhysicsAPI.h, mirroring RHI/RHIBackend.h: the
     * config data type names the enum and must not drag the whole seam in behind it.
     */
    enum class EPhysicsBackend : Uint8
    {
        Box2D
    };

    // =============================================================================
    // Backend naming
    // =============================================================================
    /**
     * Human-readable name for logs, and the label this enum is WRITTEN as in a config. Found by
     * ADL — every engine enum spells this ToString (I11).
     *
     * OPAAX_API because EngineConfigData::Physics::Backend is a real enum, which puts this on
     * the path of every TU that serializes a config — the tests and the editor exe included.
     */
    OPAAX_API const char* ToString(EPhysicsBackend InBackend) noexcept;
}

OPAAX_ENUM_VALUES(Opaax::EPhysicsBackend, Box2D)
