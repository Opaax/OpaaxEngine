#include "ShmupGameScene.h"

#include "Core/Log/OpaaxLog.h"
#include "World/World.h"
#include "ECS/Components/TransformComponent.h"

#include "ECS/Components/VelocityComponent.h"
#include "ECS/Components/AABB2DComponent.h"
#include "ECS/Components/PlayerTagComponent.h"

void ShmupGameScene::OnLoad(Opaax::World& InWorld)
{
    OPAAX_TRACE("[ShmupGameScene] OnLoad");

    Opaax::EntityID lPlayer = InWorld.CreateEntity("Player");

    Opaax::ECS::TransformComponent lTransform;
    lTransform.Position = { -400.f, 0.f };
    lTransform.Scale    = { 1.f, 1.f };
    InWorld.AddComponent<Opaax::ECS::TransformComponent>(lPlayer, lTransform);

    InWorld.AddComponent<VelocityComponent>(lPlayer);
    InWorld.AddComponent<PlayerTagComponent>(lPlayer);

    AABB2DComponent lAabb;
    lAabb.HalfExtents = { 16.f, 16.f };
    InWorld.AddComponent<AABB2DComponent>(lPlayer, lAabb);
}
