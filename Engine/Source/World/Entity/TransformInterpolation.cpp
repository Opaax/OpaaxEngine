#include "World/Entity/TransformInterpolation.h"

#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/World.h"

namespace Opaax
{
    void TransformInterpolation::RecordSteps(Entity InEntity)
    {
        if (!InEntity.IsValid()) { return; }

        // An entity physics or the mover already records keeps what they wrote.
        TransformInterpolationComponent* lHistory = InEntity.TryGet<TransformInterpolationComponent>();
        if (lHistory == nullptr)
        {
            lHistory = &InEntity.Add<TransformInterpolationComponent>();
        }

        lHistory->bRecordSteps = true;
    }

    void TransformInterpolation::BeginFixedStep(World& InWorld)
    {
        InWorld.Each<TransformComponent, TransformInterpolationComponent>(
            [](EntityID, const TransformComponent& InLocal, TransformInterpolationComponent& InHistory)
            {
                if (!InHistory.bRecordSteps) { return; }

                InHistory.Position     = InLocal.Position;
                InHistory.Rotation     = InLocal.Rotation;
                InHistory.bHasPrevious = true;
            });
    }

    void TransformInterpolation::EndFixedStep(World& InWorld)
    {
        InWorld.Each<TransformComponent, TransformInterpolationComponent>(
            [](EntityID, const TransformComponent& InLocal, TransformInterpolationComponent& InHistory)
            {
                if (!InHistory.bRecordSteps) { return; }

                InHistory.SteppedPosition = InLocal.Position;
                InHistory.SteppedRotation = InLocal.Rotation;
                InHistory.bHasStepped     = true;
            });
    }
}
