#pragma once

#include "Assets/AssetHandle.hpp"
#include "Core/Component/OpaaxComponent.h"
#include "Core/OpaaxMathTypes.h"
#include "Physics/PhysicsTypes.h"
#include "Physics/Collision/CollisionChannel.h"

namespace Opaax
{
    class CollisionProfile;
}

namespace Opaax::ECS
{
    using json = nlohmann::json;

    // =============================================================================
    // ColliderComponent
    // =============================================================================
    /**
     * @struct ColliderComponent
     *
     * The collision geometry attached to an entity, paired with a RigidbodyComponent
     * (a collider without a rigidbody is built as an implicit static body). Shape selects
     * which dimension field is read: Box uses Size (full world-unit extents), Circle uses
     * Radius. Offset shifts the shape from the entity origin in local space.
     *
     * Mode (Solid | Trigger) decides whether the shape blocks or just senses. Channel is
     * the collider's object-type category; the CollisionProfile that resolves the per-channel
     * filter mask is a later, additive field (P2b) — until then a collider blocks everything
     * on its channel.
     */
    struct OPAAX_API ColliderComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        ColliderComponent()          = default;
        explicit ColliderComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~ColliderComponent() = default;

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
        EColliderShape    Shape   = EColliderShape::Box;
        EColliderMode     Mode    = EColliderMode::Solid;

        // Object-type category. Used as the filter category directly when Profile is unset;
        // when a Profile is assigned, the profile's channel + response matrix drive the filter.
        ECollisionChannel Channel = ECollisionChannel::WorldDynamic;

        // Optional collision behaviour asset. When valid+loaded, supplies the filter
        // category (its channel) and mask (its response matrix); otherwise the collider
        // uses Channel as category and collides with everything.
        TAssetHandle<CollisionProfile> Profile;

        // Local-space shift of the shape from the entity origin, in world units.
        Vector2F          Offset  = { 0.f, 0.f };

        // Box: full extents (width, height) in world units. Unused for Circle.
        Vector2F          Size    = { 100.f, 100.f };

        // Circle: radius in world units. Unused for Box.
        float             Radius  = 50.f;

        // Material — density (mass per area, dynamic bodies), surface friction [0..1],
        // and bounciness (restitution [0..1], 0 = no bounce).
        float             Density     = 1.f;
        float             Friction    = 0.3f;
        float             Restitution = 0.f;
    };
}
