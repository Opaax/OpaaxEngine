#include "LifetimeSystem.h"

#include <vector>

#include "Core/CoreEngineApp.h"
#include "World/World.h"
#include "ECS/Components/LifetimeComponent.h"

void LifetimeSystem::Update(double DeltaTime)
{
    Opaax::World& lWorld = GetEngineApp()->GetWorld();
    const float lDt = static_cast<float>(DeltaTime);

    std::vector<Opaax::EntityID> lDoomed;

    lWorld.Each<LifetimeComponent>(
        [lDt, &lDoomed](Opaax::EntityID InEntity, LifetimeComponent& InLifetime)
        {
            InLifetime.SecondsRemaining -= lDt;
            if (InLifetime.SecondsRemaining <= 0.f)
            {
                lDoomed.push_back(InEntity);
            }
        });

    for (Opaax::EntityID lEntity : lDoomed)
    {
        lWorld.DestroyEntity(lEntity);
    }
}
