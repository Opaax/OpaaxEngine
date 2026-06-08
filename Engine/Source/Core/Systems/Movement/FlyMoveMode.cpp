#include "FlyMoveMode.h"

#include "Physics/IPhysicsWorld.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"

namespace Opaax
{
    void FlyMoveMode::OnModeEnter(MoverTickContext& InContext)
    {
        // Drop any momentum carried in from the previous mode (e.g. fall speed from GroundMove).
        InContext.Mover.Velocity = { 0.f, 0.f };
    }

    void FlyMoveMode::Tick(MoverTickContext& InContext)
    {
        ECS::MoverComponent&     lMover     = InContext.Mover;
        ECS::TransformComponent& lTransform = InContext.Transform;

        // Free flight: intent maps straight to velocity, no gravity/friction. MoveDir is treated as
        // a [-1..1] per-axis throttle; the capsule sweep still resolves collisions (slides on walls).
        const Vector2F lVelocity = lMover.Input.MoveDir * lMover.Params.MaxSpeed;

        MoveCapsuleInput lInput;
        lInput.Position      = lTransform.Position;
        lInput.Capsule       = lMover.BuildCapsule();
        lInput.Velocity      = lVelocity;
        lInput.DeltaTime     = InContext.DeltaTime;
        lInput.ChannelMask   = lMover.CollisionMask;
        lInput.MaxIterations = 5;
        lInput.GroundNormalY = lMover.GroundNormalY();

        const MoveCapsuleResult lResult = InContext.World.MoveCapsule(lInput);

        lTransform.Position = lResult.Position;
        lMover.Velocity     = lResult.Velocity;
        lMover.Grounded     = lResult.Grounded;
        lMover.GroundNormal = lResult.GroundNormal;
    }

} // namespace Opaax
