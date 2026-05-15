#include "SimpleShmupGame.h"

#include "Core/Log/OpaaxLog.h"
#include "Scene/SceneManager.h"
#include "ECS/ComponentRegistry.h"

#include "Scene/ShmupGameScene.h"
#include "Systems/PlayerControlSystem.h"
#include "Systems/MovementSystem.h"
#include "Systems/LifetimeSystem.h"

#include "ECS/Components/PlayerTagComponent.h"
#include "ECS/Components/EnemyTagComponent.h"
#include "ECS/Components/BulletTagComponent.h"
#include "ECS/Components/VelocityComponent.h"
#include "ECS/Components/LifetimeComponent.h"
#include "ECS/Components/AABB2DComponent.h"
#include "ECS/Components/ScoreComponent.h"

SimpleShmupGame::SimpleShmupGame(int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - SimpleShmup Start");
    OPAAX_TRACE("==================================");
}

void SimpleShmupGame::OnInitialize()
{
    OPAAX_TRACE("[SimpleShmupGame] OnInitialize");

    // Component registration — engine built-ins are registered in CoreEngineApp::Initialize;
    // game-side types register here so the SceneSerializer (incl. PIE snapshot) and the
    // inspector know they exist. Order is the json-key write order for serialized scenes.
    Opaax::ComponentRegistry::Register<PlayerTagComponent>("PlayerTagComponent");
    Opaax::ComponentRegistry::Register<EnemyTagComponent> ("EnemyTagComponent");
    Opaax::ComponentRegistry::Register<BulletTagComponent>("BulletTagComponent");
    Opaax::ComponentRegistry::Register<VelocityComponent> ("VelocityComponent");
    Opaax::ComponentRegistry::Register<LifetimeComponent> ("LifetimeComponent");
    Opaax::ComponentRegistry::Register<AABB2DComponent>   ("AABB2DComponent");
    Opaax::ComponentRegistry::Register<ScoreComponent>    ("ScoreComponent");

    // Game subsystems — registration order is tick order: input-driven velocity
    // write → integration → reaping.
    RegisterGameSubsystem<PlayerControlSystem>(this);
    RegisterGameSubsystem<MovementSystem>(this);
    RegisterGameSubsystem<LifetimeSystem>(this);
}

void SimpleShmupGame::OnStartup()
{
    OPAAX_TRACE("[SimpleShmupGame] OnStartup");

    GetSceneManager()->Push(Opaax::MakeUnique<ShmupGameScene>());
}

void SimpleShmupGame::OnUpdate(double DeltaTime)
{
}

void SimpleShmupGame::OnShutdown()
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("[Opaax Engine] [SimpleShmupGame] Shutdown");
    OPAAX_TRACE("==================================");
}
