#pragma once

#include <box2d/id.h>

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // Box2DPhysicsWorld
    // =============================================================================
    /**
     * @class Box2DPhysicsWorld
     *
     * Box2D 3.x implementation of IPhysicsWorld — the only place b2* appears above the
     * grep gate. Owns one b2WorldId for its lifetime. World<->Box2D unit and vector
     * conversions live entirely in the .cpp and never leak through the interface.
     */
    class Box2DPhysicsWorld final : public IPhysicsWorld
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        explicit Box2DPhysicsWorld(const PhysicsWorldDesc& InDesc);
        ~Box2DPhysicsWorld() override;

        Box2DPhysicsWorld(const Box2DPhysicsWorld&)            = delete;
        Box2DPhysicsWorld& operator=(const Box2DPhysicsWorld&) = delete;
        Box2DPhysicsWorld(Box2DPhysicsWorld&&)                 = delete;
        Box2DPhysicsWorld& operator=(Box2DPhysicsWorld&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IPhysicsWorld Interface
    public:
        void     Step(float DeltaTime, int SubStepCount) override;
        void     SetGravity(Vector2F InGravity) override;
        Vector2F GetGravity() const override;

        BodyHandle  CreateBody(const BodyDesc& InDesc) override;
        void        DestroyBody(BodyHandle InBody) override;
        ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) override;
        void        GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition, float& OutRotation) const override;
        void        SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) override;
        void        SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition, float InRotation,
                                           float InDeltaTime) override;

        void GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                             TDynArray<PhysicsContactPair>& OutEnded) override;
        void GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                              TDynArray<PhysicsContactPair>& OutEnded) override;

        PhysicsRayHit RayCastClosest(Vector2F Origin, Vector2F Direction, float Distance,
                                     Uint64 ChannelMask) override;
        void OverlapAABB(Vector2F Min, Vector2F Max, Uint64 ChannelMask,
                         TDynArray<Uint64>& OutUserData) override;

        MoveCapsuleResult MoveCapsule(const MoveCapsuleInput& InInput) override;
        //~End IPhysicsWorld Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        b2WorldId m_WorldId;
    };

} // namespace Opaax
