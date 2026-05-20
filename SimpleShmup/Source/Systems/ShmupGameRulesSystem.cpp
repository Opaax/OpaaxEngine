#include "ShmupGameRulesSystem.h"

#include <unordered_set>

#include "CollisionSystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"
#include "World/World.h"

#include "ECS/Components/BulletTagComponent.h"
#include "ECS/Components/EnemyTagComponent.h"
#include "ECS/Components/PlayerTagComponent.h"
#include "ECS/Components/ScoreComponent.h"

using namespace Opaax;

void ShmupGameRulesSystem::Update(double /*DeltaTime*/)
{
    if (m_Collision == nullptr)
    {
        m_Collision = GetEngineApp()->GetGameSubsystem<CollisionSystem>();
    }

    World& lWorld = GetEngineApp()->GetWorld();

    std::unordered_set<EntityID> lDoomed;
    Uint32 lKillsThisFrame = 0;

    for (const CollisionSystem::HitPair& lPair : m_Collision->GetHits())
    {
        const bool lAIsBullet = lWorld.HasComponent<BulletTagComponent>(lPair.A);
        const bool lAIsEnemy  = lWorld.HasComponent<EnemyTagComponent>(lPair.A);
        const bool lAIsPlayer = lWorld.HasComponent<PlayerTagComponent>(lPair.A);
        const bool lBIsBullet = lWorld.HasComponent<BulletTagComponent>(lPair.B);
        const bool lBIsEnemy  = lWorld.HasComponent<EnemyTagComponent>(lPair.B);
        const bool lBIsPlayer = lWorld.HasComponent<PlayerTagComponent>(lPair.B);

        if ((lAIsBullet && lBIsEnemy) || (lAIsEnemy && lBIsBullet))
        {
            lDoomed.insert(lPair.A);
            lDoomed.insert(lPair.B);
            ++lKillsThisFrame;
            continue;
        }

        if ((lAIsPlayer && lBIsEnemy) || (lAIsEnemy && lBIsPlayer))
        {
            // Detected; reaction deferred to G2 (no game-over this slice).
            OPAAX_TRACE("[ShmupRules] Player<->Enemy collision detected (ignored this slice).");
            continue;
        }
    }

    if (lKillsThisFrame > 0)
    {
        const Uint32 lAward = ScorePerKill * lKillsThisFrame;
        lWorld.Each<ScoreComponent>(
            [lAward](EntityID, ScoreComponent& InScore)
            {
                InScore.Score += lAward;
            });
    }

    for (EntityID lEntity : lDoomed)
    {
        lWorld.DestroyEntity(lEntity);
    }
}
