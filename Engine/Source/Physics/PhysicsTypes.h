#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // World creation
    // =============================================================================
    /**
     * @struct PhysicsWorldDesc
     *
     * Neutral creation parameters for a physics world. All values are in WORLD units
     * (Y-up); LengthUnitsPerMeter tells the backend how many world units make a metre so
     * its internal tuning (sleep thresholds, speculative margins) stays sane.
     */
    struct PhysicsWorldDesc
    {
        /** Acceleration applied to dynamic bodies, world units / s^2 (Y-up: negative falls). */
        Vector2F Gravity = { 0.f, -981.f };

        /** World units per metre. 2D convention: ~100 units = 1 m. */
        float LengthUnitsPerMeter = 100.f;

        /** Solver sub-steps per Step call. Higher = stabler stacks, more cost. */
        int SubStepCount = 4;
    };

    // =============================================================================
    // Opaque handles
    // =============================================================================
    /**
     * @struct BodyHandle
     *
     * Opaque reference to a physics body. The backend packs its own id into Id; neutral
     * code only ever copies it and checks IsValid, so nothing above the seam learns the
     * backend's handle type.
     */
    struct BodyHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    /**
     * @struct ShapeHandle
     *
     * Opaque reference to a collision shape (see BodyHandle).
     */
    struct ShapeHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    // =============================================================================
    // Body / shape enums — shared by the component layer and the backend
    // =============================================================================
    /**
     * @enum EBodyType
     * Simulation class of a rigid body. Static never moves (infinite mass); Kinematic moves
     * only when driven by code (ignores forces, pushes dynamics); Dynamic is fully simulated.
     */
    enum class EBodyType : Uint8
    {
        Static,
        Kinematic,
        Dynamic
    };

    /**
     * @enum EColliderShape
     * Primitive a collider approximates its entity with. Box uses HalfExtents; Circle uses
     * Radius; Capsule uses Center1/Center2 + Radius.
     */
    enum class EColliderShape : Uint8
    {
        Box,
        Circle,
        Capsule
    };

    /**
     * @enum EColliderMode
     * How a collider participates in the solve. Solid blocks (collision response); Overlap
     * passes through and fires overlap events. Orthogonal to the collider's CHANNEL, which
     * says what it is rather than how it reacts.
     */
    enum class EColliderMode : Uint8
    {
        Solid,
        Overlap
    };

    /**
     * @enum EWorldBoundsResponse
     * What the engine does when a dynamic body leaves the world-bounds kill volume. The
     * exit event ALWAYS fires; this selects only the engine's own follow-up.
     */
    enum class EWorldBoundsResponse : Uint8
    {
        EventOnly,
        EventAndDestroy
    };

    // =============================================================================
    // String mapping — closed enums serialize as NAMES, so map files stay readable and
    //   tolerate the enum being appended to (mirrors SpriteComponent's layer names).
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

    inline EBodyType BodyTypeFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Kinematic") { return EBodyType::Kinematic; }
        if (InName == "Dynamic")   { return EBodyType::Dynamic; }
        return EBodyType::Static;
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

    inline EColliderShape ColliderShapeFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Circle")  { return EColliderShape::Circle; }
        if (InName == "Capsule") { return EColliderShape::Capsule; }
        return EColliderShape::Box;
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

    inline EColliderMode ColliderModeFromString(const OpaaxString& InName) noexcept
    {
        // "Trigger" kept as an alias: M9-era scenes were authored before the Overlap rename.
        if (InName == "Overlap" || InName == "Trigger") { return EColliderMode::Overlap; }
        return EColliderMode::Solid;
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

    inline EWorldBoundsResponse WorldBoundsResponseFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "EventOnly") { return EWorldBoundsResponse::EventOnly; }
        return EWorldBoundsResponse::EventAndDestroy;
    }

    // =============================================================================
    // Body / shape creation
    // =============================================================================
    /**
     * @struct BodyDesc
     *
     * Neutral parameters for one physics body, translated from an entity's Rigidbody +
     * Transform. Position is in world units, Rotation in radians. UserData carries the
     * packed EntityID so contacts and queries resolve back to the owning entity.
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
     * @struct ShapeGeometry
     *
     * The shape's FORM, separate from its material and filter. Type selects which fields are
     * read; Offset is the local centre for all of them. Adding a primitive means a new
     * EColliderShape value plus the fields it needs here plus one case in the backend — no
     * ripple through ShapeDesc or its consumers.
     */
    struct ShapeGeometry
    {
        EColliderShape Type = EColliderShape::Box;

        /** Local centre offset from the body origin, world units (every shape). */
        Vector2F Offset = { 0.f, 0.f };

        /** Box: half width and half height, world units. */
        Vector2F HalfExtents = { 50.f, 50.f };

        /** Circle / capsule end-cap radius, world units. */
        float Radius = 50.f;

        /** Capsule: the two semicircle centres, local and relative to Offset. */
        Vector2F Center1 = { 0.f, 0.f };
        Vector2F Center2 = { 0.f, 0.f };
    };

    /**
     * @struct ShapeDesc
     *
     * Neutral parameters to attach one collision shape to a body: the geometry, its material,
     * and its collision filter. bIsSensor maps EColliderMode::Overlap. CategoryBits is the
     * collider's own channel bit and MaskBits the set of channels it interacts with.
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
     * @struct PhysicsContactPair
     *
     * One begin-or-end touch pair, resolved by the backend from shape -> body -> user-data to
     * the two participating entities (packed EntityID bits, as set in BodyDesc::UserData).
     * For overlap pairs A is the sensor owner and B the visitor; for solid contacts A/B follow
     * the backend's shape order.
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
     * @struct PhysicsRayHit
     *
     * Closest hit from a ray cast. UserData is the hit body's raw user-data (0 when bHit is
     * false or the shape is unresolved); the subsystem decodes it to an EntityID. Point and
     * Normal are world-space; Fraction is the [0..1] position of the hit along the ray.
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
    // Geometric mover (kinematic capsule sweep)
    // =============================================================================
    /**
     * @struct MoverCapsule
     *
     * Local-space capsule used by the geometric mover. A circle is the degenerate
     * Center1 == Center2 case. World units.
     */
    struct MoverCapsule
    {
        Vector2F Center1 = { 0.f, 0.f };
        Vector2F Center2 = { 0.f, 0.f };
        float    Radius  = 25.f;
    };

    /**
     * @struct MoveCapsuleInput
     *
     * One geometric collide-and-slide step: sweep Capsule from Position by Velocity*DeltaTime
     * against the world, iterating the plane solver up to MaxIterations. GroundNormalY is the
     * minimum surface-normal Y that counts as ground (cosine of the max walkable slope).
     * No movement policy here — gravity, acceleration and jump live in the engine-side mode.
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
         * Body user-data to skip during the sweep — the mover's OWN body, since it is a real
         * kinematic body in the world. 0 ignores nothing. Encoded like BodyDesc::UserData.
         */
        Uint64 IgnoreUserData = 0;
    };

    /**
     * @struct MoveCapsuleResult
     *
     * Post-sweep state: the resolved position, the velocity clipped against the touched planes
     * (so the mover stops pushing into walls), and grounded info. World-space.
     */
    struct MoveCapsuleResult
    {
        Vector2F Position     = { 0.f, 0.f };
        Vector2F Velocity     = { 0.f, 0.f };
        bool     bGrounded    = false;
        Vector2F GroundNormal = { 0.f, 0.f };
    };
}
