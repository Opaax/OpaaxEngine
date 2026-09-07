#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"

#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // IPhysicsWorld — the backend-neutral 2D physics world, and THE SEAM.
    //
    //   Mirrors the RHI's IRHIDevice: nobody outside Physics/ calls a backend (Box2D, ...)
    //   directly, a concrete world is built through PhysicsAPI::Create, and the choice is a
    //   config string resolved once. A grep gate proves no b2* symbol appears above
    //   Physics/Box2D/.
    //
    //   The interface speaks ENGINE concepts — world units, Y-up, opaque handles. Each
    //   backend absorbs its own quirks behind these methods (poll-based events, length
    //   units, native filter representation), so none of them surface above this line.
    // =============================================================================
    class OPAAX_API IPhysicsWorld
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
         * Advance the simulation. Called from the physics subsystem's FixedUpdate at the
         * engine's fixed timestep.
         *
         * @param InDeltaTime    seconds to advance
         * @param InSubStepCount solver sub-steps for this step
         */
        virtual void Step(float InDeltaTime, int InSubStepCount) = 0;

        /** World gravity in world units / s^2 (Y-up: negative falls). */
        virtual void     SetGravity(Vector2F InGravity) = 0;
        virtual Vector2F GetGravity() const             = 0;

        // =============================================================================
        // Bodies + shapes
        // =============================================================================
    public:
        /** Create a body from neutral parameters; the handle is invalid on failure. */
        virtual BodyHandle CreateBody(const BodyDesc& InDesc) = 0;

        /** Destroy a body and all its shapes. Safe with an invalid handle (no-op). */
        virtual void DestroyBody(BodyHandle InBody) = 0;

        /** Attach one collision shape to a body. */
        virtual ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) = 0;

        /** Read a body's world transform (world units, radians) — drives dynamic Transforms. */
        virtual void GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition,
                                      float& OutRotation) const = 0;

        /** Write a body's world transform — pushes static/kinematic Transforms into the sim. */
        virtual void SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) = 0;

        /**
         * Drive a KINEMATIC body toward a target pose over InDeltaTime. It is swept during the
         * next step, so it generates contacts and pushes dynamics — unlike a teleport. The
         * mover uses this every step.
         */
        virtual void SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition,
                                            float InRotation, float InDeltaTime) = 0;

        // =============================================================================
        // Events — drained after Step
        // =============================================================================
    public:
        /**
         * Drain the sensor (overlap) begin/end pairs the last Step accumulated into the
         * caller's buffers, which are cleared then filled. A is the sensor owner, B the
         * visitor. Resolved to entity bits.
         */
        virtual void GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                     TDynArray<PhysicsContactPair>& OutEnded) = 0;

        /**
         * Drain the solid contact begin/end pairs the last Step accumulated (cleared then
         * filled). A/B follow the backend's shape order. Resolved to entity bits.
         */
        virtual void GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                      TDynArray<PhysicsContactPair>& OutEnded) = 0;

        // =============================================================================
        // Queries
        // =============================================================================
    public:
        /**
         * Closest hit along InOrigin + normalize(InDirection) * InDistance.
         *
         * @param InChannelMask which channels are hittable (one bit per CategoryBit); ~0 hits
         *   everything. The result's UserData is the hit body's raw user-data, 0 for no hit.
         */
        virtual PhysicsRayHit RayCastClosest(Vector2F InOrigin, Vector2F InDirection,
                                             float InDistance, Uint64 InChannelMask) = 0;

        /**
         * Fill OutUserData (cleared first) with every overlapping shape's body user-data inside
         * the world-space AABB [InMin..InMax], filtered by InChannelMask.
         */
        virtual void OverlapAABB(Vector2F InMin, Vector2F InMax, Uint64 InChannelMask,
                                 TDynArray<Uint64>& OutUserData) = 0;

        // =============================================================================
        // Geometric mover
        // =============================================================================
    public:
        /**
         * One kinematic collide-and-slide step for a capsule — the character-mover primitive,
         * NOT a simulated body. Sweeps the capsule against the world and returns the resolved
         * position, the clipped velocity and grounded info. Pure geometry: movement policy
         * lives engine-side in the mover mode, never here.
         */
        virtual MoveCapsuleResult MoveCapsule(const MoveCapsuleInput& InInput) = 0;
    };
}
