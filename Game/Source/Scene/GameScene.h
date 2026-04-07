#pragma once

#include "Assets/AssetHandle.hpp"
#include "Scene/Scene.h"

class GameScene final : public Opaax::Scene
{
public:
    GameScene() : Scene("GameScene") {}

    void OnLoad()   override;
    void OnUnload() override;
    void OnUpdate(double DeltaTime) override;
    void OnRender(double Alpha)     override;

private:
    float m_TotalTime = 0.0f;

    Opaax::TextureHandle m_PlayerTexture;
    Opaax::TextureHandle m_AtlasTexture;
};
