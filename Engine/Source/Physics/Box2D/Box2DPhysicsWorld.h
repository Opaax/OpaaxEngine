#pragma once

#include <box2d/id.h>

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // Box2DPhysicsWorld — IPhysicsWorld with Box2D 3.x. The only place b2* is used.
    //   Created only through PhysicsAPI::Create, so no code outside Physics/ sees Box2D.
    // =============================================================================
    class Box2DPhysicsWorld final : public IPhysicsWorld
    {
        // =============================================================================
        // CTORS - DTOR
        // =============================================================================
    public:
        explicit Box2DPhysicsWorld(const PhysicsWorldDesc& InDesc);
        ~Box2DPhysicsWorld() override;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        Box2DPhysicsWorld(const Box2DPhysicsWorld&)            = delete;
        Box2DPhysicsWorld& operator=(const Box2DPhysicsWorld&) = delete;
        Box2DPhysicsWorld(Box2DPhysicsWorld&&)                 = delete;
        Box2DPhysicsWorld& operator=(Box2DPhysicsWorld&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IPhysicsWorld Interface
    public:
        void     Step(float InDeltaTime, int InSubStepCount) override;
        void     SetGravity(Vector2F InGravity) override;
        Vector2F GetGravity() const override;
        float    GetMaxLinearSpeed() const override;

        BodyHandle  CreateBody(const BodyDesc& InDesc) override;
        void        DestroyBody(BodyHandle InBody) override;
        ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) override;
        void        GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition,
                                     float& OutRotation) const override;
        void        SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) override;
        void        SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition, float InRotation,
                                           float InDeltaTime) override;

        Vector2F GetLinearVelocity(BodyHandle InBody) const override;
        void     SetLinearVelocity(BodyHandle InBody, Vector2F InVelocity) override;
        float    GetAngularVelocity(BodyHandle InBody) const override;
        void     SetAngularVelocity(BodyHandle InBody, float InRadiansPerSecond) override;
        void     ApplyForce(BodyHandle InBody, Vector2F InForce) override;
        void     ApplyLinearImpulse(BodyHandle InBody, Vector2F InImpulse) override;
        void     ApplyTorque(BodyHandle InBody, float InTorque) override;
        void     ApplyAngularImpulse(BodyHandle InBody, float InImpulse) override;
        float    GetMass(BodyHandle InBody) const override;

        void GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                             TDynArray<PhysicsContactPair>& OutEnded) override;
        void GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                              TDynArray<PhysicsContactPair>& OutEnded) override;

        PhysicsRayHit RayCastClosest(Vector2F InOrigin, Vector2F InDirection, float InDistance,
                                     Uint64 InChannelMask) override;
        void          OverlapAABB(Vector2F InMin, Vector2F InMax, Uint64 InChannelMask,
                                  TDynArray<Uint64>& OutUserData) override;

        MoveCapsuleResult MoveCapsule(const MoveCapsuleInput& InInput) override;
        //~End IPhysicsWorld Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        b2WorldId m_WorldId;
    };
}
