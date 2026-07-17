#pragma once

#include "Scene/Scene.h"

/**
 * @class ShmupGameScene
 *
 * Owns the player entity for the G1 vertical slice. Spawns it in OnLoad
 * (auto-tagged with the active SceneID by World::CreateEntity — dies on
 * scene unload). No per-frame tick: gameplay logic lives in game subsystems
 * registered by SimpleShmupGame::OnInitialize.
 */
class ShmupGameScene final : public Opaax::Scene
{
    // =============================================================================
    // CTORs
    // =============================================================================
public:
    ShmupGameScene() : Scene("ShmupGame") {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin Scene interface
public:
    void OnLoad(Opaax::World& InWorld) override;
    //~End Scene interface
};
