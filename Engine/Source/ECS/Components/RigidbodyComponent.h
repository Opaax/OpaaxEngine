#pragma once

#include "Core/Component/OpaaxComponent.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;

    // =============================================================================
    // RigidbodyComponent
    // =============================================================================
    /**
     * @struct RigidbodyComponent
     *
     * Marks an entity as participating in the physics simulation and authors its body's
     * simulation properties. The collider geometry lives on a sibling ColliderComponent;
     * an entity needs both to be built into the world (a body without a collider has no
     * shape; a collider without a body becomes implicitly static).
     *
     * Authoring-only data — the live BodyHandle is held by the backend, keyed by EntityID,
     * and never serialized. Dynamic bodies write their Transform back each step; static and
     * kinematic bodies read their Transform once at build.
     */
    struct OPAAX_API RigidbodyComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        RigidbodyComponent()          = default;
        explicit RigidbodyComponent(EBodyType InType) : Type(InType) {}
        explicit RigidbodyComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~RigidbodyComponent() = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin OpaaxComponentBase Interface
    protected:
        void DeserializeImplementation(const json& Json) override;

    public:
        json Serialize() const override;
        //~ End OpaaxComponentBase Interface

        // =============================================================================
        // Members
        // =============================================================================
        EBodyType Type           = EBodyType::Dynamic;

        // Per-body multiplier on world gravity (0 = floats, 1 = full, >1 = heavy). Dynamic only.
        float     GravityScale   = 1.f;

        // Lock rotation so the body slides/falls without spinning — the usual choice for
        // platformer pawns and most 2D sprites.
        bool      FixedRotation  = false;

        // Velocity bleed-off per second (0 = none). Linear damps translation, angular spin.
        float     LinearDamping  = 0.f;
        float     AngularDamping = 0.f;
    };
}
