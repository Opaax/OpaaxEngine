#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // RigidbodyComponent — how an entity is simulated (needs a ColliderComponent).
    //   Optional: a collider without a rigidbody is static.
    // =============================================================================
    struct RigidbodyComponent
    {
        /** Static never moves, Kinematic moves only when driven, Dynamic is fully simulated. */
        EBodyType Type = EBodyType::Dynamic;

        /** Gravity multiplier for this body. 0 floats; negative falls upward. */
        float GravityScale = 1.f;

        /** Locks rotation (e.g. a platformer character). */
        bool bFixedRotation = false;

        /** Velocity loss per second. */
        float LinearDamping  = 0.f;
        float AngularDamping = 0.f;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RigidbodyComponent,
                                                    Type, GravityScale, bFixedRotation,
                                                    LinearDamping, AngularDamping)

        OPAAX_PROPERTIES(RigidbodyComponent,
                         OPAAX_PROP(Type).SetTooltip("Static never moves.\n"
                                                     "Kinematic moves only when code drives it.\n"
                                                     "Dynamic is simulated: gravity, forces, contacts."),
                         OPAAX_PROP(GravityScale).SetRange(-10.f, 10.f)
                                                 .SetTooltip("Multiplies world gravity for this body.\n"
                                                             "0 floats. Ignored unless Dynamic."),
                         OPAAX_PROP(bFixedRotation).SetTooltip("Lock rotation. What a character wants."),
                         OPAAX_PROP(LinearDamping).SetRange(0.f, 10.f),
                         OPAAX_PROP(AngularDamping).SetRange(0.f, 10.f))
    };
}
