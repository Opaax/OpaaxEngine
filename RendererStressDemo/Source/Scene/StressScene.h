#pragma once

#include "Scene/Scene.h"

/**
 * @class StressScene
 *
 * Empty scene whose only job is to BE the active scene — the WorldRenderPass only dispatches
 * IWorldSystems when a scene is active. All BulletStorm content is drawn procedurally by
 * BulletStormRenderSystem, not authored entities, so this scene needs no entities or overrides.
 */
class StressScene final : public Opaax::Scene
{
public:
    StressScene() : Scene("StressScene") {}
};
