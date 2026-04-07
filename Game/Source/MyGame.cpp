#include "MyGame.h"
#include <iostream>

#include "Assets/AssetRegistry.h"
#include "Core/Input/InputSubsystem.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/World/WorldRenderSystem.h"
#include "ECS/BaseComponents.hpp"
#include "Renderer/Renderer2D.h"
#include "RHI/RenderCommand.h"
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

void MyGame::OnRender(double AlphaPhysicStep)
{
    auto* lCamera = GetSubsystem<Opaax::Camera2D>();

    Opaax::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    Opaax::RenderCommand::Clear();

    Opaax::Renderer2D::Begin(*lCamera);

    if (auto* lScene = GetSceneManager()->GetActiveScene())
    {
        Opaax::WorldRenderSystem::Render(lScene->GetWorld());
    }

    Opaax::Renderer2D::End();
}
