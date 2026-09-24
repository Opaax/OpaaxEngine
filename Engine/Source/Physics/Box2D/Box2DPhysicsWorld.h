#pragma once

#include <box2d/id.h>

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // Box2DPhysicsWorld — the Box2D 3.x implementation of IPhysicsWorld.
    //
    //   The ONLY place b2* appears above the grep gate. Owns one b2WorldId for its lifetime;
    //   world <-> Box2D unit and vector conversions live entirely in the .cpp and never leak
    //   through the interface.
    //
    //   NOT OPAAX_API and never named outside Physics/: it is reached only through
    //   PhysicsAPI::Create, which is what keeps box2d PRIVATE to the engine DLL (L11 —
    //   a static vendor lib linked PUBLIC gives every host its own copy of the vendor's
    //   global state, and b2SetLengthUnitsPerMeter is exactly that).
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

        BodyHandle  CreateBody(const BodyDesc& InDesc) override;
        void        DestroyBody(BodyHandle InBody) override;
        ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) override;
        void        GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition,
                                     float& OutRotation) const override;
        void        SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) override;
        void        SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition, float InRotation,
                                           float InDeltaTime) override;

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
