#pragma once

#include "Scene/Scene.h"

/**
 * @class MenuScene
 *
 * Empty title screen for the G1 vertical slice. Spawns no entities. The
 * Menu→Game transition is owned by SceneTransitionSystem (game subsystem),
 * which listens for Space-just-pressed while this scene is active.
 */
class MenuScene final : public Opaax::Scene
{
    // =============================================================================
    // CTORs
    // =============================================================================
public:
    MenuScene() : Scene("Menu") {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin Scene interface
public:
    void OnLoad(Opaax::World& InWorld) override;
    //~End Scene interface
};
