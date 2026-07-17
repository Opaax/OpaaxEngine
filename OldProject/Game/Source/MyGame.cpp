#include "MyGame.h"

#include <iostream>

#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxTypes.h"
#include "Scene/GameScene.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"
#include "Systems/PlayerDemoSystem.h"

MyGame::MyGame(int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - Game Start");
    OPAAX_TRACE("==================================");
}

void MyGame::OnInitialize()
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("[Opaax Engine] [MyGame] Initialize - Game starting!");
    OPAAX_TRACE("==================================");

    // Register scene factories — required for PIE Stop to rebuild the stack.
    Opaax::SceneFactory::Register<GameScene>("GameScene");

    RegisterGameSubsystem<PlayerDemoSystem>(this);
}

void MyGame::OnStartup()
{
    OPAAX_TRACE("[MyGame] OnStartup");
    GetSubsystem<Opaax::SceneManager>()->Push(Opaax::MakeUnique<GameScene>());
}

void MyGame::OnUpdate(double deltaTime)
{
}

void MyGame::OnShutdown()
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("[Opaax Engine] [MyGame] Shutdown - Game Ending!");
    OPAAX_TRACE("==================================");
}
