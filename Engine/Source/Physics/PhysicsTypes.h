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
        Circle
    };

    // ---------------------------------------------------------------------------
    /**
     * @enum EColliderMode
     * How a collider participates in the solve. Solid blocks (collision response);
     * Trigger overlaps without response and fires sensor events. Maps to the backend's
     * sensor flag — the WHAT (channel) stays orthogonal to this HOW.
     */
    enum class EColliderMode : Uint8
    {
        Solid,
        Trigger
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
            case EColliderShape::Box:    return "Box";
            case EColliderShape::Circle: return "Circle";
        }
        return "Box";
    }

    inline EColliderShape ColliderShapeFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Circle") { return EColliderShape::Circle; }
        return EColliderShape::Box;
    }

    inline const char* ToString(EColliderMode InMode) noexcept
    {
        switch (InMode)
        {
            case EColliderMode::Solid:   return "Solid";
            case EColliderMode::Trigger: return "Trigger";
        }
        return "Solid";
    }

    inline EColliderMode ColliderModeFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Trigger") { return EColliderMode::Trigger; }
        return EColliderMode::Solid;
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

        // Circle (and future Capsule end-cap): radius in world units.
        float          Radius      = 50.f;
    };

    // ---------------------------------------------------------------------------
    /**
     * @struct ShapeDesc
     *
     * Backend-neutral parameters to attach one collision shape to a body, translated from an
     * entity's ColliderComponent: the Geometry (form) plus its material and collision filter.
     * IsSensor maps Mode==Trigger. CategoryBits/MaskBits default to "collide with all" and are
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

} // namespace Opaax
