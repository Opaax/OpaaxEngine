#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // RigidbodyComponent — HOW an entity is simulated. It carries no shape and no position:
    //   the collider beside it is the shape, and TransformComponent is the position (I17).
    //
    //   IT IS OPTIONAL, AND ITS ABSENCE MEANS SOMETHING. An entity with a collider and no
    //   rigidbody is STATIC — level geometry, which is the overwhelmingly common case and
    //   should not need a second component to say so. Adding one is what makes a thing move.
    //
    //   Alone it does nothing at all: PhysicsSubsystem builds a body per COLLIDER, and reads
    //   this to decide what kind. A rigidbody with no collider is a body with no shape, which
    //   nothing can touch and gravity moves invisibly — so it is skipped, deliberately, rather
    //   than made an error.
    // =============================================================================
    struct RigidbodyComponent
    {
        /** Static never moves, Kinematic moves only when driven, Dynamic is fully simulated. */
        EBodyType Type = EBodyType::Dynamic;

        /** Multiplies world gravity for this body alone. 0 floats; negative falls upward. */
        float GravityScale = 1.f;

        /** Lock rotation. What a platformer character wants — a capsule that never tips over. */
        bool bFixedRotation = false;

        /** Velocity bleed per second. 0 keeps moving forever (in a frictionless direction). */
        float LinearDamping  = 0.f;
        float AngularDamping = 0.f;

        // _WITH_DEFAULT is required, not preferred: the plain macro reads every field with at(),
        // which THROWS on a missing key, so adding a field would refuse every map saved before it.
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
