#include "GameScene.h"

#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Scene/SceneSerializer.h"

void GameScene::OnLoad(Opaax::World& InWorld)
{
    OPAAX_TRACE("[GameScene] OnLoad");

    // Try to load
    const Opaax::OpaaxString lSavePath = Opaax::OpaaxPath::ToAbsolute("Game/Assets/Scenes/GameScene.scene.json");

    if (std::filesystem::exists(lSavePath.CStr()))
    {
        OPAAX_TRACE("[GameScene] Loading from file: {}", lSavePath);
        Opaax::SceneSerializer::Deserialize(*this, lSavePath.CStr(), InWorld);
        SetSourcePath(lSavePath);
        return;
    }

    // Default scene
    OPAAX_TRACE("[GameScene] No save file found, building scene from code.");
    BuildDefaultScene(InWorld);
}

void GameScene::OnUnload(Opaax::World& InWorld)
{
#if OPAAX_WITH_EDITOR
    // Editor builds: the user owns scene persistence via the File menu / Ctrl+S.
    // Auto-saving here would overwrite the hardcoded GameScene.scene.json regardless of
    // what the user did with New/Open/Save As, which silently nukes their work.
    OPAAX_TRACE("[GameScene] OnUnload — editor build, skipping implicit save.");
#else
    OPAAX_TRACE("[GameScene] OnUnload — saving scene.");
    SaveScene(InWorld);
#endif

    m_PlayerTexture.Reset();
    m_AtlasTexture.Reset();
}

void GameScene::OnUpdate(double DeltaTime)
{
    m_TotalTime += static_cast<float>(DeltaTime);
}

void GameScene::SaveScene(Opaax::World& InWorld)
{
    const Opaax::OpaaxString lSavePath = Opaax::OpaaxPath::ToAbsolute("Game/Assets/Scenes/GameScene.scene.json");

    // Create File
    std::filesystem::create_directories(
        std::filesystem::path(lSavePath.CStr()).parent_path());

    Opaax::SceneSerializer::Serialize(*this, lSavePath.CStr(), InWorld);
}

void GameScene::BuildDefaultScene(Opaax::World& InWorld)
{
    m_PlayerTexture = Opaax::AssetRegistry::Load<Opaax::Texture2D>(OPAAX_ID("Textures/Player"));
    m_AtlasTexture  = Opaax::AssetRegistry::Load<Opaax::Texture2D>(OPAAX_ID("Textures/PlayerSheet"));

    auto lPlayer = InWorld.CreateEntity("Player");
    InWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer,
        Opaax::ECS::TransformComponent{ {0.f, 0.f}, {64.f, 64.f} });
    Opaax::ECS::SpriteComponent& lSP1 = InWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPlayer, Opaax::ECS::SpriteComponent{m_PlayerTexture});

    auto lAtlas = InWorld.CreateEntity("AtlasSprite");
    InWorld.AddComponent<Opaax::ECS::TransformComponent>(lAtlas, Opaax::ECS::TransformComponent{ {100.f, 0.f}, {64.f, 64.f} });
    Opaax::ECS::SpriteComponent& lSP = InWorld.AddComponent<Opaax::ECS::SpriteComponent>(lAtlas, Opaax::ECS::SpriteComponent{m_AtlasTexture, {0.f,   0.f}, {0.25f, 0.25f}});

    auto lPersistant = InWorld.CreatePersistentEntity("Persistant_Debug");
    InWorld.AddComponent<Opaax::ECS::TransformComponent>(lPersistant, Opaax::ECS::TransformComponent{ {-400.f, 0.f}, {1.f, 1.f} });
    lSP = InWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPersistant, Opaax::ECS::SpriteComponent{m_AtlasTexture, {0.f,   0.f}, {0.25f, 0.25f}});
    lSP.Size = { 64.f, 64.f };
}
