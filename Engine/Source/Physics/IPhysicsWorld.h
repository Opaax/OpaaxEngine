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
    };

} // namespace Opaax
