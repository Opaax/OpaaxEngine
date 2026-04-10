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
#include "Editor/EditorSubsystem.h"

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
#if OPAAX_WITH_EDITOR
    // En mode editor, on render dans le FBO du viewport panel
    auto* lEditor = GetSubsystem<Opaax::EditorSubsystem>();

    lEditor->GetViewport().BindFBO();

    Opaax::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    Opaax::RenderCommand::Clear();

    auto* lCamera = GetSubsystem<Opaax::Camera2D>();
    Opaax::Renderer2D::Begin(*lCamera);

    if (auto* lScene = GetSubsystem<Opaax::SceneManager>()->GetActiveScene())
    {
        Opaax::WorldRenderSystem::Render(lScene->GetWorld());
    }

    Opaax::Renderer2D::End();

    lEditor->GetViewport().UnbindFBO();

#else
    
    auto* lCamera = GetSubsystem<Opaax::Camera2D>();

    Opaax::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    Opaax::RenderCommand::Clear();

    Opaax::Renderer2D::Begin(*lCamera);

    if (auto* lScene = GetSceneManager()->GetActiveScene())
    {
        Opaax::WorldRenderSystem::Render(lScene->GetWorld());
    }

    Opaax::Renderer2D::End();
#endif
}
