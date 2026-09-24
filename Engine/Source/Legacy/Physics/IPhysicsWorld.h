#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"

#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // IPhysicsWorld
    // =============================================================================
    /**
     * @interface IPhysicsWorld
     *
     * Backend-neutral 2D physics world — the seam, mirroring the RHI's IRenderAPI.
     * Nobody outside Physics/ calls a physics backend (Box2D, ...) directly; a concrete
     * world is built through PhysicsAPI::Create and owned by the PhysicsSubsystem.
     *
     * The interface speaks ENGINE concepts (world units, Y-up, neutral handles). Each
     * backend absorbs its own quirks behind these methods — poll-based events, length
     * units, native filter representation — so they never surface above this line.
     *
     * Body / shape / query / event / character-mover methods are layered on in later
     * phases; this interface stays minimal until a consumer needs each one.
     */
    class OPAAX_API IPhysicsWorld
    {
    public:
        virtual ~IPhysicsWorld() = default;

        // Advance the simulation by DeltaTime seconds using SubStepCount solver sub-steps.
        // Called from PhysicsSubsystem::FixedUpdate at the engine's fixed timestep.
        virtual void     Step(float DeltaTime, int SubStepCount) = 0;

        // World gravity in world units / s^2 (Y-up: negative falls).
        virtual void     SetGravity(Vector2F InGravity) = 0;
        virtual Vector2F GetGravity() const             = 0;

        // -------------------------------------------------------------------------
        // Bodies + shapes
        // -------------------------------------------------------------------------
        // Create a body from neutral parameters; returns an opaque handle (invalid on failure).
        virtual BodyHandle  CreateBody(const BodyDesc& InDesc)             = 0;

        // Destroy a body and all its shapes. Safe to call with an invalid handle (no-op).
        virtual void        DestroyBody(BodyHandle InBody)                = 0;

        // Attach one collision shape to a body; returns an opaque shape handle.
        virtual ShapeHandle AddShape(BodyHandle InBody, const ShapeDesc& InShape) = 0;

        // Read a body's world transform (world units, radians) — used to drive dynamic Transforms.
        virtual void        GetBodyTransform(BodyHandle InBody, Vector2F& OutPosition, float& OutRotation) const = 0;

        // Write a body's world transform — used to push static/kinematic ECS Transforms into the sim.
        virtual void        SetBodyTransform(BodyHandle InBody, Vector2F InPosition, float InRotation) = 0;

        // Drive a KINEMATIC body toward a target pose over DeltaTime (sweeps it during the next step so
        // it generates contacts + pushes dynamics, unlike a teleport). Used by the mover each step.
        virtual void        SetBodyTargetTransform(BodyHandle InBody, Vector2F InPosition, float InRotation,
                                                   float InDeltaTime) = 0;

        // -------------------------------------------------------------------------
        // Events (drained after Step)
        // -------------------------------------------------------------------------
        // Drain sensor (overlap) begin/end pairs accumulated by the last Step into the caller's
        // buffers (cleared then filled). A = sensor owner, B = visitor. Resolved to entity bits.
        virtual void GetSensorEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                     TDynArray<PhysicsContactPair>& OutEnded) = 0;

        // Drain solid contact begin/end pairs accumulated by the last Step (cleared then filled).
        // A/B follow the backend's shape order. Resolved to entity bits.
        virtual void GetContactEvents(TDynArray<PhysicsContactPair>& OutBegan,
                                      TDynArray<PhysicsContactPair>& OutEnded) = 0;

        // -------------------------------------------------------------------------
        // Queries
        // -------------------------------------------------------------------------
        // Closest hit along Origin + normalize(Direction) * Distance. ChannelMask selects which
        // channels are hittable (bit set per CategoryBit); ~0 hits everything. UserData in the
        // result is the hit body's raw user-data (0 = no hit / unresolved).
        virtual PhysicsRayHit RayCastClosest(Vector2F Origin, Vector2F Direction, float Distance,
                                             Uint64 ChannelMask) = 0;

        // Fill OutUserData (cleared first) with every overlapping shape's body user-data within the
        // world-space AABB [Min..Max], filtered by ChannelMask.
        virtual void OverlapAABB(Vector2F Min, Vector2F Max, Uint64 ChannelMask,
                                 TDynArray<Uint64>& OutUserData) = 0;

        // -------------------------------------------------------------------------
        // Geometric mover
        // -------------------------------------------------------------------------
        // One kinematic collide-and-slide step for a capsule (the geometric character-mover
        // primitive — NOT a simulated body). Sweeps the capsule against the world and returns the
        // resolved position, clipped velocity, and grounded info. Pure geometry: movement policy
        // (gravity/acceleration/jump) lives engine-side in the mover mode, never here.
        virtual MoveCapsuleResult MoveCapsule(const MoveCapsuleInput& InInput) = 0;
    };

} // namespace Opaax
