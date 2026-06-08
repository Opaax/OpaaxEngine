#include "GroundMoveMode.h"

#include <algorithm>
#include <cmath>

#include "Physics/IPhysicsWorld.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"

namespace Opaax
{
    namespace
    {
        // Quake-style ground/air velocity update (mirrors sample_character.cpp SolveMove). All
        // tuning comes from MoverParams — no magic numbers. Returns the new velocity to sweep with.
        Vector2F SolveVelocity(const MoverParams& InParams, Vector2F InVelocity, Vector2F InMoveDir,
                               bool InGrounded, bool InJump, float InDt) noexcept
        {
            Vector2F lVel = InVelocity;

            // --- Friction ---
            const float lSpeed = std::sqrt(lVel.x * lVel.x + lVel.y * lVel.y);
            if (lSpeed < InParams.MinSpeed)
            {
                lVel = { 0.f, 0.f };
            }
            else if (InGrounded)
            {
                const float lControl  = lSpeed < InParams.StopSpeed ? InParams.StopSpeed : lSpeed;
                const float lDrop      = lControl * InParams.Friction * InDt;
                const float lNewSpeed  = std::max(0.f, lSpeed - lDrop);
                lVel *= lNewSpeed / lSpeed;
            }

            // --- Desired horizontal velocity from intent (MoveDir.x = throttle [-1..1]) ---
            const float lThrottle = std::clamp(InMoveDir.x, -1.f, 1.f);
            float lDesiredSpeed   = std::fabs(lThrottle) * InParams.MaxSpeed;
            const Vector2F lDir   = lThrottle >= 0.f ? Vector2F{ 1.f, 0.f } : Vector2F{ -1.f, 0.f };
            lDesiredSpeed         = std::min(lDesiredSpeed, InParams.MaxSpeed);

            // On the ground, drop any residual vertical speed before accelerating + jumping.
            if (InGrounded) { lVel.y = 0.f; }

            // --- Accelerate toward the desired horizontal speed ---
            const float lCurrentSpeed = lVel.x * lDir.x + lVel.y * lDir.y;  // dot(vel, dir)
            const float lAddSpeed     = lDesiredSpeed - lCurrentSpeed;
            if (lAddSpeed > 0.f)
            {
                const float lSteer      = InGrounded ? 1.f : InParams.AirSteer;
                float       lAccelSpeed = lSteer * InParams.Acceleration * InParams.MaxSpeed * InDt;
                lAccelSpeed             = std::min(lAccelSpeed, lAddSpeed);
                lVel += lAccelSpeed * lDir;
            }

            // --- Jump (grounded only; the producer sends Jump as an edge) ---
            if (InGrounded && InJump)
            {
                lVel.y = InParams.JumpSpeed;
            }

            // --- Gravity (applied by the mode — the geometric mover is not in the sim) ---
            lVel.y -= InParams.Gravity * InDt;

            return lVel;
        }
    }

    void GroundMoveMode::Tick(MoverTickContext& InContext)
    {
        ECS::MoverComponent&     lMover     = InContext.Mover;
        ECS::TransformComponent& lTransform = InContext.Transform;
        const float              lDt        = InContext.DeltaTime;

        const Vector2F lVelocity = SolveVelocity(lMover.Params, lMover.Velocity, lMover.Input.MoveDir,
                                                 lMover.Grounded, lMover.Input.Jump, lDt);

        // Consume the jump edge so a single request fires once, regardless of fixed-step count.
        lMover.Input.Jump = false;

        MoveCapsuleInput lInput;
        lInput.Position      = lTransform.Position;
        lInput.Capsule       = lMover.BuildCapsule();
        lInput.Velocity      = lVelocity;
        lInput.DeltaTime     = lDt;
        lInput.ChannelMask   = lMover.CollisionMask;
        lInput.MaxIterations = 5;
        lInput.GroundNormalY = lMover.GroundNormalY();

        const MoveCapsuleResult lResult = InContext.World.MoveCapsule(lInput);

        lTransform.Position    = lResult.Position;
        lMover.Velocity        = lResult.Velocity;
        lMover.Grounded        = lResult.Grounded;
        lMover.GroundNormal    = lResult.GroundNormal;
    }

} // namespace Opaax
