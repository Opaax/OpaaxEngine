#include "MyGame.h"
#include <iostream>
#include "Core/Log/OpaaxLog.h"
#include "Scene/GameScene.h"
#include "Scene/SceneManager.h"
#include "Core/OpaaxTypes.h"

MyGame::MyGame():Opaax::CoreEngineApp()
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
