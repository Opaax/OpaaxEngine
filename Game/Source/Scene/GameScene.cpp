#include "GameScene.h"

#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/World/WorldRenderSystem.h"
#include "ECS/BaseComponents.hpp"
#include "Renderer/Camera2D.h"
#include "Renderer/Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "Core/Log/OpaaxLog.h"

void GameScene::OnLoad()
{
    OPAAX_TRACE("[GameScene] OnLoad");

    m_PlayerTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/Player.png"));

    m_AtlasTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/PlayerSheet.png"));

    auto& lWorld = GetWorld();

    // Player
    auto lPlayer = lWorld.CreateEntity("Player");
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer,
        Opaax::ECS::TransformComponent{ .Position = {0.f, 0.f}, .Scale = {64.f, 64.f} });
    lWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPlayer,
        Opaax::ECS::SpriteComponent{ .Texture = m_PlayerTexture });

    // Atlas sprite
    auto lAtlas = lWorld.CreateEntity("AtlasSprite");
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lAtlas,
        Opaax::ECS::TransformComponent{ .Position = {100.f, 0.f}, .Scale = {64.f, 64.f} });
    lWorld.AddComponent<Opaax::ECS::SpriteComponent>(lAtlas,
        Opaax::ECS::SpriteComponent{
            .Texture = m_AtlasTexture,
            .UVMin   = {0.f,   0.f},
            .UVMax   = {0.25f, 0.25f}
        });
}

void GameScene::OnUnload()
{
    OPAAX_TRACE("[GameScene] OnUnload");
    m_PlayerTexture.Reset();
    m_AtlasTexture.Reset();
}

void GameScene::OnUpdate(double DeltaTime)
{
    m_TotalTime += static_cast<float>(DeltaTime);
}

void GameScene::OnRender(double /*Alpha*/)
{
    Opaax::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.f);
    Opaax::RenderCommand::Clear();
}