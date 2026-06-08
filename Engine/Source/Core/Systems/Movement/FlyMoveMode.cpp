#include "FlyMoveMode.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/Systems/Movement/FlyMoveParams.h"

#include "Physics/IPhysicsWorld.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"

namespace Opaax
{
    UniquePtr<IMoverModeParams> FlyMoveMode::CreateDefaultParams() const
    {
        return MakeUnique<FlyMoveParams>();
    }

    void FlyMoveMode::OnModeEnter(MoverTickContext& InContext)
    {
        // Drop any momentum carried in from the previous mode (e.g. fall speed from GroundMove).
        InContext.Mover.Velocity = { 0.f, 0.f };
    }

    void FlyMoveMode::Tick(MoverTickContext& InContext)
    {
        OPAAX_CORE_ASSERT(InContext.Params && InContext.Params->TypeTag() == FlyMoveParams::StaticTypeTag())
        const auto* lParams = static_cast<const FlyMoveParams*>(InContext.Params);

        ECS::MoverComponent&     lMover     = InContext.Mover;
        ECS::TransformComponent& lTransform = InContext.Transform;

        // Free flight: intent maps straight to velocity, no gravity/friction. MoveDir is a [-1..1]
        // per-axis throttle; the capsule sweep still resolves collisions (slides on walls).
        const Vector2F lVelocity = lMover.Input.MoveDir * lParams->MaxSpeed;

        MoveCapsuleInput lInput;
        lInput.Position       = lTransform.Position;
        lInput.Capsule        = lMover.BuildCapsule();
        lInput.Velocity       = lVelocity;
        lInput.DeltaTime      = InContext.DeltaTime;
        lInput.ChannelMask    = lMover.EffectiveMovementMask();
        lInput.MaxIterations  = 5;
        lInput.GroundNormalY  = lMover.GroundNormalY();
        lInput.IgnoreUserData = InContext.SelfUserData;

        const MoveCapsuleResult lResult = InContext.World.MoveCapsule(lInput);

        lTransform.Position = lResult.Position;
        lMover.Velocity     = lResult.Velocity;
        lMover.Grounded     = lResult.Grounded;
        lMover.GroundNormal = lResult.GroundNormal;
    }

} // namespace Opaax
