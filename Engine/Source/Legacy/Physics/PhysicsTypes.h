#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    // =============================================================================
    // PhysicsWorldDesc
    // =============================================================================
    /**
     * @struct PhysicsWorldDesc
     *
     * Neutral creation parameters for a physics world. Backend-agnostic — a backend
     * translates these into its own world definition. All values live in WORLD units
     * (Y-up); LengthUnitsPerMeter tells the backend how many world units make a metre
     * so its internal tuning (sleep thresholds, speculative margins) stays sane.
     */
    struct PhysicsWorldDesc
    {
        // Acceleration applied to dynamic bodies, in world units / s^2 (Y-up: negative falls).
        Vector2F Gravity = { 0.f, -981.f };

        // World units per metre. Box-style 2D convention: ~100 units = 1 m.
        float    LengthUnitsPerMeter = 100.f;

        // Solver sub-steps per Step call. Higher = stabler stacks, more cost.
        int      SubStepCount = 4;
    };

    // =============================================================================
    // Opaque handles
    // =============================================================================
    /**
     * @struct BodyHandle
     *
     * Opaque, backend-agnostic reference to a physics body. The backend packs its own
     * id into Id; neutral code only ever copies it and checks IsValid. Never holds a
     * raw backend type, so nothing above the physics seam learns the backend.
     */
    struct BodyHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct ShapeHandle
     *
     * Opaque, backend-agnostic reference to a collision shape (see BodyHandle).
     */
    struct ShapeHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    // =============================================================================
    // Body / shape enums (neutral — shared by the ECS component layer and the backend)
    // =============================================================================
    /**
     * @enum EBodyType
     * Simulation class of a rigid body. Static never moves (infinite mass); Kinematic
     * moves only when driven by code (ignores forces, pushes dynamics); Dynamic is fully
     * simulated (gravity, forces, collisions). Maps to the backend's body-type concept.
     */
    enum class EBodyType : Uint8
    {
        Static,
        Kinematic,
        Dynamic
    };

    // ---------------------------------------------------------------------------
    /**
     * @enum EColliderShape
     * Primitive a collider approximates its entity with. Box uses Size (full extents);
     * Circle uses Radius. The mover capsule (P4) is a separate component, not a shape here.
     */
    enum class EColliderShape : Uint8
    {
        Box,
        Circle,
        Capsule
    };

    // ---------------------------------------------------------------------------
    /**
     * @enum EColliderMode
     * How a collider participates in the solve. Solid blocks (collision response);
     * Overlap passes through without response and fires overlap events. Maps to the backend's
     * sensor flag — the WHAT (channel) stays orthogonal to this HOW.
     */
    enum class EColliderMode : Uint8
    {
        Solid,
        Overlap
    };

    // ---------------------------------------------------------------------------
    // String mapping — closed enums serialized as names for readable, reorder-tolerant
    // scene JSON (mirrors SpriteComponent's layer-name serialization).
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
        // "Trigger" kept as a legacy alias so scenes authored before the Overlap rename still load.
        if (InName == "Overlap" || InName == "Trigger") { return EColliderMode::Overlap; }
        return EColliderMode::Solid;
    }

    // ---------------------------------------------------------------------------
    /**
     * @enum EWorldBoundsResponse
     * What the engine does when a dynamic body leaves the world-bounds kill volume. The
     * OnExitWorldBounds event ALWAYS fires; this only selects the engine's own follow-up:
     * EventOnly fires the event and nothing more (the game reacts); EventAndDestroy fires
     * the event AND reaps the entity + its body. Drives PhysicsSubsystem::EnforceWorldBounds.
     */
    enum class EWorldBoundsResponse : Uint8
    {
        EventOnly,
        EventAndDestroy
    };

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
    // BodyDesc / ShapeDesc — neutral body + shape creation parameters
    // =============================================================================
    /**
     * @struct BodyDesc
     *
     * Backend-neutral parameters to create one physics body, translated from an entity's
     * RigidbodyComponent + TransformComponent. Position is in world units (Y-up), Rotation
     * in radians. UserData carries the packed EntityID so contacts/queries resolve back to
     * the owning entity (P3).
     */
    struct BodyDesc
    {
        EBodyType Type           = EBodyType::Dynamic;
        Vector2F  Position       = { 0.f, 0.f };
        float     Rotation       = 0.f;
        float     GravityScale   = 1.f;
        bool      FixedRotation  = false;
        float     LinearDamping  = 0.f;
        float     AngularDamping = 0.f;
        Uint64    UserData       = 0;
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct ShapeGeometry
     *
     * Backend-neutral collision geometry — the shape's *form*, separate from its material
     * and filter (mirrors how Box2D itself splits a polygon/circle/capsule primitive from its
     * shape-definition struct). Type selects which fields are read; Offset is the local center for all.
     * Adding a primitive (Capsule, Polygon, ...) means a new EColliderShape value + the fields
     * it needs here + one case in the backend — no ripple through ShapeDesc or its consumers.
     */
    struct ShapeGeometry
    {
        EColliderShape Type        = EColliderShape::Box;

        // Local-space center offset from the body origin, in world units (all shapes).
        Vector2F       Offset      = { 0.f, 0.f };

        // Box: half extents (half width, half height) in world units.
        Vector2F       HalfExtents = { 50.f, 50.f };

        // Circle / Capsule end-cap: radius in world units.
        float          Radius      = 50.f;

        // Capsule: the two semicircle centers (local, relative to Offset), in world units.
        // (Circle is the degenerate Center1 == Center2 case; a dedicated Circle uses Radius only.)
        Vector2F       Center1     = { 0.f, 0.f };
        Vector2F       Center2     = { 0.f, 0.f };
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct ShapeDesc
     *
     * Backend-neutral parameters to attach one collision shape to a body, translated from an
     * entity's ColliderComponent: the Geometry (form) plus its material and collision filter.
     * IsSensor maps Mode==Overlap. CategoryBits/MaskBits default to "collide with all" and are
     * refined by the CollisionProfile.
     */
    struct ShapeDesc
    {
        ShapeGeometry Geometry;

        bool          IsSensor     = false;
        float         Density      = 1.f;
        float         Friction     = 0.3f;
        float         Restitution  = 0.f;
        Uint64        CategoryBits = ~0ull;
        Uint64        MaskBits     = ~0ull;
    };

    // =============================================================================
    // PhysicsContactPair — neutral event pair drained from the backend
    // =============================================================================
    /**
     * @struct PhysicsContactPair
     *
     * One begin-or-end touch pair, resolved by the backend from shape -> body -> user-data
     * to the two participating entities (packed EntityID bits, as set in BodyDesc::UserData).
     * For sensor (overlap) pairs A is the sensor owner and B the visitor; for solid contact
     * pairs A/B follow Box2D's shapeIdA/shapeIdB order. Neutral — never holds a backend type.
     */
    struct PhysicsContactPair
    {
        Uint64 EntityA = 0;
        Uint64 EntityB = 0;
    };

    // =============================================================================
    // PhysicsRayHit — neutral closest-ray result
    // =============================================================================
    /**
     * @struct PhysicsRayHit
     *
     * Closest hit from a ray cast. UserData is the hit body's raw user-data (0 when bHit is
     * false or the shape is unresolved); the subsystem decodes it to an EntityID. Point/Normal
     * are world-space; Fraction is the [0..1] position of the hit along the cast ray. Neutral —
     * never holds a backend type.
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
     * Local-space capsule used by the geometric mover (Box2D's capsule-only character
     * solver). A circle is a degenerate capsule with Center1 == Center2. Never holds a
     * backend type. World units.
     */
    struct MoverCapsule
    {
        Vector2F Center1 = { 0.f, 0.f };
        Vector2F Center2 = { 0.f, 0.f };
        float    Radius  = 25.f;
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct MoveCapsuleInput
     *
     * One geometric collide-and-slide step: sweep Capsule from Position by Velocity*DeltaTime
     * against the world's shapes (filtered by ChannelMask = which channels are solid to the
     * mover), iterating the plane solver up to MaxIterations. GroundNormalY is the minimum
     * surface-normal Y that counts as "ground" (cos of the max walkable slope). Neutral — no
     * movement policy here (gravity/accel/jump live in the engine-side mode).
     */
    struct MoveCapsuleInput
    {
        Vector2F     Position      = { 0.f, 0.f };
        MoverCapsule Capsule;
        Vector2F     Velocity      = { 0.f, 0.f };
        float        DeltaTime     = 0.f;
        Uint64       ChannelMask   = ~0ull;
        int          MaxIterations = 5;
        float        GroundNormalY = 0.7f;

        // Body user-data to skip during the sweep (the mover's OWN body, now that it's a real
        // kinematic body in the world). 0 = ignore nothing. Encoded entity-bits+1, as BodyDesc::UserData.
        Uint64       IgnoreUserData = 0;
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct MoveCapsuleResult
     *
     * Post-sweep state: resolved Position, the velocity clipped against the touched planes
     * (so the mover stops pushing into walls), and grounded info (Grounded true when a touched
     * plane's normal.y >= GroundNormalY; GroundNormal is that plane's normal). World-space.
     */
    struct MoveCapsuleResult
    {
        Vector2F Position     = { 0.f, 0.f };
        Vector2F Velocity     = { 0.f, 0.f };
        bool     Grounded     = false;
        Vector2F GroundNormal = { 0.f, 0.f };
    };

} // namespace Opaax
