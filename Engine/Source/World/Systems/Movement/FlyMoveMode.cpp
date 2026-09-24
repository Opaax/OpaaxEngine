#include "World/Systems/Movement/FlyMoveMode.h"

#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/IPhysicsWorld.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/TransformComponent.h"

namespace Opaax
{
    void FlyMoveMode::OnModeEnter(MoverTickContext& InContext)
    {
        // Without this, switching to flight mid-fall keeps the downward speed and the thing sinks
        // for a moment before the intent takes over.
        InContext.Mover.Velocity = { 0.f, 0.f };
    }

    void FlyMoveMode::Tick(MoverTickContext& InContext)
    {
        MoverComponent&     lMover     = InContext.Mover;
        TransformComponent& lTransform = InContext.Transform;

        // Intent straight to velocity: no acceleration curve, because a debug/noclip mode wants to
        // go exactly where it is pointed. Both axes, unlike GroundMove which reads x alone.
        const Vector2F lVelocity = lMover.Input.MoveDir * InContext.Params.MaxSpeed;

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
