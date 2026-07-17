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

void GameScene::OnUnload(Opaax::World& /*InWorld*/)
{
    // Unloading never persists the scene implicitly. A shipped game must not overwrite its
    // own scene asset on quit, and in the editor persistence is user-driven (Ctrl+S, or a
    // save-or-discard prompt). Explicit saves go through SaveScene().
    OPAAX_TRACE("[GameScene] OnUnload");

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
    InWorld.AddComponent<Opaax::ECS::SpriteComponent>(lAtlas, Opaax::ECS::SpriteComponent{m_AtlasTexture, {0.f,   0.f}, {0.25f, 0.25f}});
}
