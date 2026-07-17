#include "MovementSystem.h"

#include "Core/CoreEngineApp.h"
#include "World/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VelocityComponent.h"

void MovementSystem::Update(double DeltaTime)
{
    Opaax::World& lWorld = GetEngineApp()->GetWorld();
    const float lDt = static_cast<float>(DeltaTime);

    lWorld.Each<Opaax::ECS::TransformComponent, const VelocityComponent>(
        [lDt](Opaax::EntityID, Opaax::ECS::TransformComponent& InTransform, const VelocityComponent& InVelocity)
        {
            InTransform.Position += InVelocity.Velocity * lDt;
        });
}
