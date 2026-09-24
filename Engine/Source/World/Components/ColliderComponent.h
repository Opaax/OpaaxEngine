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
    // ColliderComponent — the SHAPE an entity occupies in the physics world, and the component
    //   that decides an entity is in it at all: PhysicsSubsystem builds one body per collider,
    //   and reads the optional RigidbodyComponent beside it only to pick the body TYPE.
    //
    //   SIZE IS THE COLLIDER'S, NOT THE SPRITE'S. A hitbox is routinely smaller than the art
    //   (and a platformer's usually is), so this never reads SpriteComponent::Size — the two
    //   are independent on purpose, the way Transform owns position and each component owns
    //   its own extent (I17's shape one level down).
    //
    //   WHICH FIELDS ARE READ DEPENDS ON Shape: Box uses Size, Circle uses Radius, Capsule uses
    //   both (Radius as the end caps, Size.y as the total height). Offset applies to all three.
    //   That is a live union rather than three components, because the alternative is three
    //   registrations and three drawers for one idea.
    //
    //   CHANNEL, NOT PROFILE. The channel says WHAT this is, and it becomes the shape's category
    //   bit; Mode says HOW it reacts. A per-channel response MATRIX is the CollisionProfile
    //   resource, deliberately not built (PH4) — ShapeDesc already takes CategoryBits/MaskBits,
    //   so it lands later as pure addition with no map migration.
    // =============================================================================
    struct ColliderComponent
    {
        /** Which primitive approximates the entity. Decides which of the fields below are read. */
        EColliderShape Shape = EColliderShape::Box;

        /** Solid blocks and reports contacts; Overlap passes through and reports overlaps. */
        EColliderMode Mode = EColliderMode::Solid;

        /** WHAT this collider is. Becomes its category bit — the unit queries filter on. */
        ECollisionChannel Channel = ECollisionChannel::WorldStatic;

        /** Local offset from the entity's Transform, world units. */
        Vector2F Offset = { 0.f, 0.f };

        /** Box: FULL width and height. Capsule: Size.y is the total height, Size.x unused. */
        Vector2F Size = { 100.f, 100.f };

        /** Circle: the radius. Capsule: the end-cap radius, and half its width. */
        float Radius = 50.f;

        // ---- material: what a contact FEELS like -----------------------------------------
        /** Mass per unit area. Ignored on a static body, which has infinite mass regardless. */
        float Density = 1.f;

        /** 0 slides forever, 1 grips hard. A platformer lives on this value. */
        float Friction = 0.3f;

        /** Bounce. 0 lands dead, 1 returns all of the impact energy. */
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
