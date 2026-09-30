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
         * Quake-style ground/air update: friction, acceleration toward the desired speed, jump, gravity.
         * Gravity is the world's vector scaled by the tuning.
         */
        Vector2F SolveVelocity(const MoveModeData& InParams, Vector2F InVelocity, Vector2F InMoveDir,
                               const bool bInGrounded, const bool bInJump, Vector2F InWorldGravity,
                               const float InDelta) noexcept
        {
            Vector2F lVel = InVelocity;

            // ---- friction, as a rate (1/s) -------------------------------------------------------
            const float lSpeed = Maths::Sqrt(lVel.x * lVel.x + lVel.y * lVel.y);

            if (lSpeed < InParams.MinSpeed)
            {
                lVel = { 0.f, 0.f };   // stop
            }
            else if (bInGrounded)
            {
                const float lControl  = lSpeed < InParams.StopSpeed ? InParams.StopSpeed : lSpeed;
                const float lDrop     = lControl * InParams.GroundDeceleration * InDelta;
                const float lNewSpeed = Maths::Max(0.f, lSpeed - lDrop);

                lVel *= lNewSpeed / lSpeed;
            }

            // ---- desired horizontal speed ---------------------------------------------------------
            const float    lThrottle     = Maths::Clamp(InMoveDir.x, -1.f, 1.f);
            const Vector2F lDir          = lThrottle >= 0.f ? Vector2F{ 1.f, 0.f } : Vector2F{ -1.f, 0.f };
            const float    lDesiredSpeed = Maths::Min(Maths::Abs(lThrottle) * InParams.MaxSpeed,
                                                      InParams.MaxSpeed);

            // On the ground: stop the fall speed.
            if (bInGrounded) { lVel.y = 0.f; }

            // ---- accelerate, with less control in the air -----------------------------------------
            const float lCurrentSpeed = lVel.x * lDir.x + lVel.y * lDir.y;
            const float lAddSpeed     = lDesiredSpeed - lCurrentSpeed;

            if (lAddSpeed > 0.f)
            {
                const float lSteer      = bInGrounded ? 1.f : InParams.AirSteer;
                const float lAccelSpeed = Maths::Min(lSteer * InParams.Acceleration * InParams.MaxSpeed * InDelta,
                                                     lAddSpeed);

                lVel += lAccelSpeed * lDir;
            }

            // ---- jump, only when grounded ---------------------------------------------------------
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

        // Consume the jump request here, so it fires once per request.
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
