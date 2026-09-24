#include "World/Systems/Movement/GroundMoveMode.h"

#include "Core/Maths/Maths.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Physics/IPhysicsWorld.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/TransformComponent.h"

namespace Opaax
{
    namespace
    {
        /**
         * Quake-style ground/air velocity update — friction, then acceleration toward the desired
         * speed, then the jump, then gravity. Gravity is the world's own vector scaled per tuning,
         * so direction comes from the world and only the magnitude is a per-mover decision.
         */
        Vector2F SolveVelocity(const MoveModeData& InParams, Vector2F InVelocity, Vector2F InMoveDir,
                               const bool bInGrounded, const bool bInJump, Vector2F InWorldGravity,
                               const float InDelta) noexcept
        {
            Vector2F lVel = InVelocity;

            // ---- friction, as a RATE (1/s) rather than a surface coefficient -----------------
            const float lSpeed = Maths::Sqrt(lVel.x * lVel.x + lVel.y * lVel.y);

            if (lSpeed < InParams.MinSpeed)
            {
                lVel = { 0.f, 0.f };   // snapped, so it never creeps at a pixel a second
            }
            else if (bInGrounded)
            {
                const float lControl  = lSpeed < InParams.StopSpeed ? InParams.StopSpeed : lSpeed;
                const float lDrop     = lControl * InParams.GroundDeceleration * InDelta;
                const float lNewSpeed = Maths::Max(0.f, lSpeed - lDrop);

                lVel *= lNewSpeed / lSpeed;
            }

            // ---- the horizontal speed the intent is asking for --------------------------------
            const float    lThrottle     = Maths::Clamp(InMoveDir.x, -1.f, 1.f);
            const Vector2F lDir          = lThrottle >= 0.f ? Vector2F{ 1.f, 0.f } : Vector2F{ -1.f, 0.f };
            const float    lDesiredSpeed = Maths::Min(Maths::Abs(lThrottle) * InParams.MaxSpeed,
                                                      InParams.MaxSpeed);

            // Standing on something means the fall is over; without this the downward velocity
            // accumulates while grounded and the first step off a ledge is a lurch.
            if (bInGrounded) { lVel.y = 0.f; }

            // ---- accelerate toward it, with less authority in the air -------------------------
            const float lCurrentSpeed = lVel.x * lDir.x + lVel.y * lDir.y;
            const float lAddSpeed     = lDesiredSpeed - lCurrentSpeed;

            if (lAddSpeed > 0.f)
            {
                const float lSteer      = bInGrounded ? 1.f : InParams.AirSteer;
                const float lAccelSpeed = Maths::Min(lSteer * InParams.Acceleration * InParams.MaxSpeed * InDelta,
                                                     lAddSpeed);

                lVel += lAccelSpeed * lDir;
            }

            // ---- the jump, spent only when there is something to push off --------------------
            if (bInGrounded && bInJump)
            {
                lVel.y = InParams.JumpSpeed;
            }

            lVel += InWorldGravity * InParams.GravityScale * InDelta;

            return lVel;
        }
    }

    void GroundMoveMode::Tick(MoverTickContext& InContext)
    {
        MoverComponent&     lMover     = InContext.Mover;
        TransformComponent& lTransform = InContext.Transform;

        const Vector2F lVelocity = SolveVelocity(InContext.Params, lMover.Velocity, lMover.Input.MoveDir,
                                                 lMover.bGrounded, lMover.Input.bJump,
                                                 InContext.World.GetGravity(), InContext.DeltaTime);

        // The jump edge is consumed HERE, so one request fires once however many fixed steps the
        // frame ran — a held key that re-armed every step would be a different verb.
        lMover.Input.bJump = false;

        MoveCapsuleInput lInput;
        lInput.Position       = lTransform.Position;
        lInput.Capsule        = lMover.BuildCapsule();
        lInput.Velocity       = lVelocity;
        lInput.DeltaTime      = InContext.DeltaTime;
        lInput.ChannelMask    = AllChannelsMask();
        lInput.MaxIterations  = 5;
        lInput.GroundNormalY  = InContext.Params.GroundNormalY();
        lInput.IgnoreUserData = InContext.SelfUserData;

        const MoveCapsuleResult lResult = InContext.World.MoveCapsule(lInput);

        lTransform.Position = lResult.Position;
        lMover.Velocity     = lResult.Velocity;
        lMover.bGrounded    = lResult.bGrounded;
        lMover.GroundNormal = lResult.GroundNormal;
    }
}
