#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES

namespace Opaax
{
    // =============================================================================
    // World creation
    // =============================================================================
    /**
     * Physics world settings, in world units (Y-up). LengthUnitsPerMeter tells the backend how many
     * world units make a metre.
     */
    struct PhysicsWorldDesc
    {
        /** Acceleration on dynamic bodies, world units / s^2 (Y-up: negative falls). */
        Vector2F Gravity = { 0.f, -981.f };

        /** World units per metre (~100 in 2D). */
        float LengthUnitsPerMeter = 100.f;

        /** Solver sub-steps per Step. Higher is more stable but costs more. */
        int SubStepCount = 4;
    };

    // =============================================================================
    // Opaque handles
    // =============================================================================
    /**
     * Opaque physics body handle. Only copy it and check IsValid.
     */
    struct BodyHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    /**
     * Opaque collision shape handle.
     */
    struct ShapeHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    // =============================================================================
    // Body / shape enums
    // =============================================================================
    /**
     * Static never moves; Kinematic moves only when driven by code (pushes dynamic bodies);
     * Dynamic is fully simulated.
     */
    enum class EBodyType : Uint8
    {
        Static,
        Kinematic,
        Dynamic
    };

    /**
     * Collider shape. Box uses HalfExtents; Circle uses Radius; Capsule uses Center1/Center2 + Radius.
     */
    enum class EColliderShape : Uint8
    {
        Box,
        Circle,
        Capsule
    };

    /**
     * Solid blocks; Overlap passes through and fires overlap events.
     */
    enum class EColliderMode : Uint8
    {
        Solid,
        Overlap
    };

    /**
     * What the engine does when a dynamic body leaves the world bounds (the event always fires).
     */
    enum class EWorldBoundsResponse : Uint8
    {
        EventOnly,
        EventAndDestroy
    };

    // =============================================================================
    // Enum to string. All these enums declare their values (bottom of the file), so they are
    // saved by name and shown as dropdowns.
    // =============================================================================
    inline const char* ToString(EBodyType InType) noexcept
    {
        switch (InType)
        {
            case EBodyType::Static:    return "Static";
            case EBodyType::Kinematic: return "Kinematic";
            case EBodyType::Dynamic:   return "Dynamic";
        }
        return "Static";
    }

    inline const char* ToString(EColliderShape InShape) noexcept
    {
        switch (InShape)
        {
            case EColliderShape::Box:     return "Box";
            case EColliderShape::Circle:  return "Circle";
            case EColliderShape::Capsule: return "Capsule";
        }
        return "Box";
    }

    inline const char* ToString(EColliderMode InMode) noexcept
    {
        switch (InMode)
        {
            case EColliderMode::Solid:   return "Solid";
            case EColliderMode::Overlap: return "Overlap";
        }
        return "Solid";
    }

    inline const char* ToString(EWorldBoundsResponse InResponse) noexcept
    {
        switch (InResponse)
        {
            case EWorldBoundsResponse::EventOnly:       return "EventOnly";
            case EWorldBoundsResponse::EventAndDestroy: return "EventAndDestroy";
        }
        return "EventAndDestroy";
    }

    // =============================================================================
    // Body / shape creation
    // =============================================================================
    /**
     * One physics body, from an entity's Rigidbody and Transform. Position in world units, rotation
     * in radians. UserData is the packed EntityID (to find the entity from contacts and queries).
     */
    struct BodyDesc
    {
        EBodyType Type           = EBodyType::Dynamic;
        Vector2F  Position       = { 0.f, 0.f };
        float     Rotation       = 0.f;
        float     GravityScale   = 1.f;
        bool      bFixedRotation = false;
        float     LinearDamping  = 0.f;
        float     AngularDamping = 0.f;
        Uint64    UserData       = 0;
    };

    /**
     * A shape's geometry. Type selects which fields are used; Offset applies to all.
     */
    struct ShapeGeometry
    {
        EColliderShape Type = EColliderShape::Box;

        /** Local offset from the body origin, world units. */
        Vector2F Offset = { 0.f, 0.f };

        /** Box: half width and half height, world units. */
        Vector2F HalfExtents = { 50.f, 50.f };

        /** Circle / capsule cap radius, world units. */
        float Radius = 50.f;

        /** Capsule: the two cap centres, relative to Offset. */
        Vector2F Center1 = { 0.f, 0.f };
        Vector2F Center2 = { 0.f, 0.f };
    };

    /**
     * One shape: geometry, material and filter. bIsSensor = Overlap mode. CategoryBits is the
     * collider's channel bit; MaskBits the channels it interacts with.
     */
    struct ShapeDesc
    {
        ShapeGeometry Geometry;

        bool   bIsSensor    = false;
        float  Density      = 1.f;
        float  Friction     = 0.3f;
        float  Restitution  = 0.f;
        Uint64 CategoryBits = ~0ull;
        Uint64 MaskBits     = ~0ull;
    };

    // =============================================================================
    // Events
    // =============================================================================
    /**
     * One begin or end touch, as entity bits (BodyDesc::UserData). For overlaps A is the sensor
     * owner and B the visitor.
     */
    struct PhysicsContactPair
    {
        Uint64 EntityA = 0;
        Uint64 EntityB = 0;
    };

    // =============================================================================
    // Queries
    // =============================================================================
    /**
     * Closest ray hit. UserData is the hit body's user data (0 when no hit). Point and Normal in
     * world space; Fraction is the position along the ray [0..1].
     */
    struct PhysicsRayHit
    {
        bool     bHit     = false;
        Uint64   UserData = 0;
        Vector2F Point    = { 0.f, 0.f };
        Vector2F Normal   = { 0.f, 0.f };
        float    Fraction = 0.f;
    };

    // =============================================================================
    // Mover (kinematic capsule sweep)
    // =============================================================================
    /**
     * The mover's capsule in local space (Center1 == Center2 is a circle). World units.
     */
    struct MoverCapsule
    {
        Vector2F Center1 = { 0.f, 0.f };
        Vector2F Center2 = { 0.f, 0.f };
        float    Radius  = 25.f;
    };

    /**
     * One collide-and-slide step: sweeps Capsule from Position by Velocity*DeltaTime, up to
     * MaxIterations. GroundNormalY is the minimum normal Y that counts as ground (cos of the
     * max slope).
     */
    struct MoveCapsuleInput
    {
        Vector2F     Position = { 0.f, 0.f };
        MoverCapsule Capsule;
        Vector2F     Velocity      = { 0.f, 0.f };
        float        DeltaTime     = 0.f;
        Uint64       ChannelMask   = ~0ull;
        int          MaxIterations = 5;
        float        GroundNormalY = 0.7f;

        /**
         * Body user data to ignore (the mover's own body). 0 ignores nothing.
         */
        Uint64 IgnoreUserData = 0;
    };

    /**
     * Result: new position, velocity clipped against the touched planes, and ground info.
     */
    struct MoveCapsuleResult
    {
        Vector2F Position     = { 0.f, 0.f };
        Vector2F Velocity     = { 0.f, 0.f };
        bool     bGrounded    = false;
        Vector2F GroundNormal = { 0.f, 0.f };
    };
}

OPAAX_ENUM_VALUES(Opaax::EBodyType, Static, Kinematic, Dynamic);
OPAAX_ENUM_VALUES(Opaax::EColliderShape, Box, Circle, Capsule);
OPAAX_ENUM_VALUES(Opaax::EColliderMode, Solid, Overlap);
OPAAX_ENUM_VALUES(Opaax::EWorldBoundsResponse, EventOnly, EventAndDestroy);
