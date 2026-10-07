#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"

#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // IPhysicsWorld — backend-neutral 2D physics world. Nothing outside Physics/ calls a backend
    //   directly; worlds are created through PhysicsAPI::Create. Works in world units, Y-up,
    //   with opaque handles.
    // =============================================================================
    class IPhysicsWorld
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IPhysicsWorld() = default;

        // =============================================================================
        // Simulation
        // =============================================================================
    public:
        /**
         * Advances the simulation (from the physics subsystem's FixedUpdate).
         * @param InDeltaTime    Seconds to advance
         * @param InSubStepCount Solver sub-steps
         */
        virtual void Step(float InDeltaTime, int InSubStepCount) = 0;

        /** World gravity in world units / s^2 (Y-up: negative falls). */
        virtual void     SetGravity(Vector2F InGravity) = 0;
        virtual Vector2F GetGravity() const             = 0;

        /** The fastest a body may move, world units / s: a longer move in one step is not swept. */
        virtual float GetMaxLinearSpeed() const = 0;

        // =============================================================================
        // Bodies + shapes
        // =============================================================================
    public:
        /** Creates a body. The handle is invalid on failure. */
        virtual BodyHandle CreateBody(const BodyDesc& InDesc) = 0;

        /** Destroys a body and its shapes. Does nothing for an invalid handle. */
        virtual void DestroyBody(BodyHandle InBody) = 0;

        /** Adds one collision shape to a body. */
        virtual ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) = 0;

        /** Reads a body's world transform (world units, radians). */
        virtual void GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition,
                                      float& OutRotation) const = 0;

        /** Writes a body's world transform (teleport). */
        virtual void SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) = 0;

        /**
         * Moves a kinematic body toward a target pose over InDeltaTime (swept next step, so it
         * creates contacts and pushes dynamic bodies). Used by the mover every step.
         */
        virtual void SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition,
                                            float InRotation, float InDeltaTime) = 0;

        // =============================================================================
        // Body motion — for dynamic bodies. A stale handle reads zero and ignores writes.
        // =============================================================================
    public:
        /** World units per second. */
        virtual Vector2F GetLinearVelocity(BodyHandle InBody) const = 0;
        virtual void     SetLinearVelocity(BodyHandle InBody, Vector2F InVelocity) = 0;

        /** Radians per second, counter-clockwise. */
        virtual float GetAngularVelocity(BodyHandle InBody) const = 0;
        virtual void  SetAngularVelocity(BodyHandle InBody, float InRadiansPerSecond) = 0;

        /** A force at the centre of mass, over the next step (mass * units / s^2). Wakes the body. */
        virtual void ApplyForce(BodyHandle InBody, Vector2F InForce) = 0;

        /** An instant change of momentum at the centre of mass (mass * units / s). Wakes the body. */
        virtual void ApplyLinearImpulse(BodyHandle InBody, Vector2F InImpulse) = 0;

        /** Over the next step. Wakes the body. */
        virtual void ApplyTorque(BodyHandle InBody, float InTorque) = 0;

        /** Instant. Wakes the body. */
        virtual void ApplyAngularImpulse(BodyHandle InBody, float InImpulse) = 0;

        /** From the shapes' density and area. Zero for static and kinematic bodies. */
        virtual float GetMass(BodyHandle InBody) const = 0;

        // =============================================================================
        // Events — read after Step
        // =============================================================================
    public:
        /**
         * The sensor (overlap) begin/end pairs from the last Step. The buffers are cleared then filled.
         * A is the sensor owner, B the visitor. Entity bits.
         */
        virtual void GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                     TDynArray<PhysicsContactPair>& OutEnded) = 0;

        /**
         * The solid contact begin/end pairs from the last Step (cleared then filled). Entity bits.
         */
        virtual void GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                      TDynArray<PhysicsContactPair>& OutEnded) = 0;

        // =============================================================================
        // Queries
        // =============================================================================
    public:
        /**
         * Closest hit along InDirection, up to InDistance.
         * @param InChannelMask Hittable channels (CategoryBit values); ~0 = all.
         *   The result's UserData is the hit body's user data, 0 for no hit.
         */
        virtual PhysicsRayHit RayCastClosest(Vector2F InOrigin, Vector2F InDirection,
                                             float InDistance, Uint64 InChannelMask) = 0;

        /**
         * Fills OutUserData (cleared first) with the body user data of every shape overlapping the
         * box [InMin..InMax], filtered by InChannelMask.
         */
        virtual void OverlapAABB(Vector2F InMin, Vector2F InMax, Uint64 InChannelMask,
                                 TDynArray<Uint64>& OutUserData) = 0;

        // =============================================================================
        // Geometric mover
        // =============================================================================
    public:
        /**
         * One collide-and-slide step for a capsule (not a simulated body). Returns the new position,
         * the clipped velocity and ground info. Movement rules live in the mover modes.
         */
        virtual MoveCapsuleResult MoveCapsule(const MoveCapsuleInput& InInput) = 0;
    };
}
