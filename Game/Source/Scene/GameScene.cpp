#include "GameScene.h"

#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/World/WorldRenderSystem.h"
#include "ECS/BaseComponents.hpp"
#include "Renderer/Camera2D.h"
#include "Renderer/Renderer2D.h"
#include "RHI/RenderCommand.h"
#include "Core/Log/OpaaxLog.h"
#include "Scene/SceneSerializer.h"

void GameScene::OnLoad()
{
    OPAAX_TRACE("[GameScene] OnLoad");

    // Try to load 
    const Opaax::OpaaxString lSavePath = Opaax::OpaaxPath::Resolve("GameAssets/Scenes/GameScene.json");

    if (std::filesystem::exists(lSavePath.CStr()))
    {
        OPAAX_TRACE("[GameScene] Loading from file: {}", lSavePath);
        Opaax::SceneSerializer::Deserialize(*this, lSavePath.CStr());
        return;
    }

    // Default scene
    OPAAX_TRACE("[GameScene] No save file found, building scene from code.");
    BuildDefaultScene();
}

void GameScene::OnUnload()
{
    OPAAX_TRACE("[GameScene] OnUnload — saving scene.");

    SaveScene();
    
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

void GameScene::SaveScene()
{
    const Opaax::OpaaxString lSavePath = Opaax::OpaaxPath::Resolve("GameAssets/Scenes/GameScene.json");

    // Create File 
    std::filesystem::create_directories(
        std::filesystem::path(lSavePath.CStr()).parent_path());

    Opaax::SceneSerializer::Serialize(*this, lSavePath.CStr());
}

void GameScene::BuildDefaultScene()
{
    m_PlayerTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/Player.png"));

    m_AtlasTexture = Opaax::AssetRegistry::Load<Opaax::OpenGLTexture2D>(
        OPAAX_ASSET("EngineAssets/Textures/PlayerSheet.png"));

    auto& lWorld = GetWorld();

    auto lPlayer = lWorld.CreateEntity("Player");
    lWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer,
        Opaax::ECS::TransformComponent{ .Position = {0.f, 0.f}, .Scale = {64.f, 64.f} });
    lWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPlayer,
        Opaax::ECS::SpriteComponent{ .Texture = m_PlayerTexture });

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
