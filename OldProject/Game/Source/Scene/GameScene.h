#pragma once

#include "Assets/AssetHandle.hpp"
#include "Scene/Scene.h"

class GameScene final : public Opaax::Scene
{
public:
    GameScene() : Scene("GameScene") {}

    void OnLoad(Opaax::World& InWorld)                  override;
    void OnUnload(Opaax::World& InWorld)                override;
    void OnUpdate(double DeltaTime)                     override;
    void SaveScene(Opaax::World& InWorld)               override;

    void BuildDefaultScene(Opaax::World& InWorld);

private:
    float m_TotalTime = 0.0f;

    Opaax::TextureHandle m_PlayerTexture;
    Opaax::TextureHandle m_AtlasTexture;
};