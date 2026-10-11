#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // ColliderComponent — the shape an entity occupies in the physics world.
    //   PhysicsSubsystem creates one body per collider; an optional RigidbodyComponent sets its type.
    //   Independent from the sprite's size (hitboxes are often smaller than the art).
    //   Box uses Size, Circle uses Radius, Capsule uses both (Radius for the caps, Size.y for the height).
    //   Channel is what it is (its category bit); Mode is how it reacts.
    // =============================================================================
    struct ColliderComponent
    {
        /** The shape type. Decides which fields below are used. */
        EColliderShape Shape = EColliderShape::Box;

        /** Solid blocks and reports contacts; Overlap passes through and reports overlaps. */
        EColliderMode Mode = EColliderMode::Solid;

        /** What this collider is (its category bit, used by queries). */
        ECollisionChannel Channel = ECollisionChannel::WorldStatic;

        /** Local offset from the entity's Transform, world units. */
        Vector2F Offset = { 0.f, 0.f };

        /** Box: full width and height. Capsule: Size.y is the total height, Size.x unused. */
        Vector2F Size = { 100.f, 100.f };

        /** Circle: the radius. Capsule: the end-cap radius. */
        float Radius = 50.f;

        // ---- material --------------------------------------------------------------------------
        /** Mass per unit area. Ignored on a static body. */
        float Density = 1.f;

        /** 0 slides forever, 1 grips hard. */
        float Friction = 0.3f;

        /** Bounce. 0 does not bounce, 1 keeps all the energy. */
        float Restitution = 0.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ColliderComponent,
                                                    Shape, Mode, Channel, Offset, Size, Radius,
                                                    Density, Friction, Restitution)

        OPAAX_PROPERTIES(ColliderComponent,
                         OPAAX_PROP(Shape).SetTooltip("Box reads Size. Circle reads Radius.\n"
                                                      "Capsule reads both."),
                         OPAAX_PROP(Mode).SetTooltip("Solid blocks and reports contacts.\n"
                                                     "Overlap passes through and reports overlaps."),
                         OPAAX_PROP(Channel).SetTooltip("WHAT this collider is.\n"
                                                        "Queries filter on it; Mode decides how it reacts."),
                         OPAAX_PROP(Offset),
                         OPAAX_PROP(Size).SetRange(1.f, 4096.f)
                                         .SetTooltip("FULL width and height, not half extents.\n"
                                                     "Independent of the sprite: a hitbox is\n"
                                                     "usually smaller than the art."),
                         OPAAX_PROP(Radius).SetRange(1.f, 2048.f),
                         OPAAX_PROP(Density).SetRange(0.f, 100.f),
                         OPAAX_PROP(Friction).SetRange(0.f, 1.f),
                         OPAAX_PROP(Restitution).SetRange(0.f, 1.f))
    };
}
