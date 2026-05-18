#include "EnemySpawnSystem.h"

#include "Core/CoreEngineApp.h"
#include "Scene/SceneManager.h"
#include "World/World.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VelocityComponent.h"
#include "ECS/Components/LifetimeComponent.h"
#include "ECS/Components/AABB2DComponent.h"
#include "ECS/Components/EnemyTagComponent.h"

using namespace Opaax;

EnemySpawnSystem::EnemySpawnSystem()
    : m_Rng(std::random_device{}())
{}

EnemySpawnSystem::EnemySpawnSystem(CoreEngineApp* InEngineApp)
    : GameSubsystemBase(InEngineApp)
    , m_Rng(std::random_device{}())
{}

void EnemySpawnSystem::Update(double DeltaTime)
{
    // Only spawn while the gameplay scene is active. In MenuScene (or any other
    // scene) the spawner is dormant — reset the timer so the first spawn after
    // entering ShmupGameScene is paced by SpawnInterval, not "everything queued
    // up while we were idle".
    SceneManager* lSceneMgr = GetEngineApp()->GetSceneManager();
    Scene* lActive = lSceneMgr ? lSceneMgr->GetActiveScene() : nullptr;
    if (!lActive || lActive->GetName() != "ShmupGame")
    {
        m_SpawnTimer = 0.f;
        return;
    }

    World& lWorld = GetEngineApp()->GetWorld();

    m_SpawnTimer += static_cast<float>(DeltaTime);
    if (m_SpawnTimer < SpawnInterval) return;
    // Preserve leftover time so jitter doesn't drift the cadence.
    m_SpawnTimer -= SpawnInterval;

    Uint32 lAlive = 0;
    lWorld.Each<EnemyTagComponent>(
        [&lAlive](EntityID, EnemyTagComponent&) { ++lAlive; });

    if (lAlive >= MaxAlive) return;

    std::uniform_real_distribution<float> lYDist(SpawnYRange.x, SpawnYRange.y);
    const float lY = lYDist(m_Rng);

    EntityID lEnemy = lWorld.CreateEntity("Enemy");

    Opaax::ECS::TransformComponent lTransform;
    lTransform.Position = { SpawnX, lY };
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lEnemy, lTransform);

    VelocityComponent lVel;
    lVel.Velocity = { -EnemySpeed, 0.f };
    lWorld.AddComponent<VelocityComponent>(lEnemy, lVel);

    LifetimeComponent lLife;
    lLife.SecondsRemaining = EnemyLifetime;
    lWorld.AddComponent<LifetimeComponent>(lEnemy, lLife);

    AABB2DComponent lAabb;
    lAabb.HalfExtents = { 16.f, 16.f };
    lWorld.AddComponent<AABB2DComponent>(lEnemy, lAabb);

    lWorld.AddComponent<EnemyTagComponent>(lEnemy);
}
