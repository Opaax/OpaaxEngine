#include "ShmupGameScene.h"

#include "Assets/AssetRegistry.h"
#include "Core/Log/OpaaxLog.h"
#include "World/World.h"
#include "ECS/Components/TransformComponent.h"

#include "ECS/Components/VelocityComponent.h"
#include "ECS/Components/AABB2DComponent.h"
#include "ECS/Components/PlayerTagComponent.h"
#include "ECS/Components/ScoreComponent.h"
#include "ECS/Components/SpriteComponent.h"

void ShmupGameScene::OnLoad(Opaax::World& InWorld)
{
    OPAAX_TRACE("[ShmupGameScene] OnLoad");

    Opaax::Uint32 lScore = 0;
    InWorld.Each<ScoreComponent>(
        [&lScore](Opaax::EntityID, const ScoreComponent& InScore)
        {
            lScore = InScore.Score;
        });

    OPAAX_INFO("[ShmupGameScene] Score on entry: {}", lScore);

    Opaax::EntityID lPlayer = InWorld.CreateEntity("Player");

    Opaax::ECS::TransformComponent lTransform;
    lTransform.Position = { -400.f, 0.f };
    lTransform.Scale    = { 1.f, 1.f };
    InWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer, lTransform);

    Opaax::TAssetHandle<Opaax::Texture2D> lPlayerTexture = Opaax::AssetRegistry::Load<Opaax::Texture2D>(OPAAX_ID("Textures/Player"));

    Opaax::ECS::SpriteComponent lPlayerSprite;
    lPlayerSprite.Visible = true;
    lPlayerSprite.Size    = { 145,145 };
    lPlayerSprite.Texture = lPlayerTexture;
    
    InWorld.AddComponent<Opaax::ECS::SpriteComponent>(lPlayer, lPlayerSprite);

    InWorld.AddComponent<VelocityComponent>(lPlayer);
    InWorld.AddComponent<PlayerTagComponent>(lPlayer);

    AABB2DComponent lAabb;
    lAabb.HalfExtents = { 16.f, 16.f };
    InWorld.AddComponent<AABB2DComponent>(lPlayer, lAabb);
}
