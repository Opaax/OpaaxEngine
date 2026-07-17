#include "CollisionSystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/OpaaxMathTypes.h"
#include "World/World.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/AABB2DComponent.h"

using namespace Opaax;

namespace
{
    struct Collider
    {
        EntityID Entity;
        Vector2F Center;
        Vector2F HalfExtents;
    };

    bool AABBOverlap(const Collider& A, const Collider& B) noexcept
    {
        const float lDx = A.Center.x - B.Center.x;
        const float lDy = A.Center.y - B.Center.y;
        const float lSumX = A.HalfExtents.x + B.HalfExtents.x;
        const float lSumY = A.HalfExtents.y + B.HalfExtents.y;
        return (lDx < 0 ? -lDx : lDx) <= lSumX
            && (lDy < 0 ? -lDy : lDy) <= lSumY;
    }
}

void CollisionSystem::Update(double /*DeltaTime*/)
{
    m_Hits.clear();

    World& lWorld = GetEngineApp()->GetWorld();

    std::vector<Collider> lColliders;
    lWorld.Each<Opaax::ECS::TransformComponent, AABB2DComponent>(
        [&lColliders](EntityID InEntity, Opaax::ECS::TransformComponent& InTransform, AABB2DComponent& InAabb)
        {
            Collider lCollider;
            lCollider.Entity      = InEntity;
            lCollider.Center      = InTransform.Position + InAabb.Offset;
            lCollider.HalfExtents = InAabb.HalfExtents;
            lColliders.push_back(lCollider);
        });

    const size_t lCount = lColliders.size();
    for (size_t i = 0; i + 1 < lCount; ++i)
    {
        for (size_t j = i + 1; j < lCount; ++j)
        {
            if (AABBOverlap(lColliders[i], lColliders[j]))
            {
                m_Hits.push_back({ lColliders[i].Entity, lColliders[j].Entity });
            }
        }
    }
}
