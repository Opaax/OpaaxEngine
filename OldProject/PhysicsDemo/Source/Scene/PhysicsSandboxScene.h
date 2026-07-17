#pragma once

#include "Assets/AssetHandle.hpp"
#include "Scene/Scene.h"

/**
 * @class PhysicsSandboxScene
 *
 * One scene that lays out every M9 physics feature side by side (world ≈ ±640 × ±360 at
 * the default camera, Y-up, gravity down):
 *   - two ground slabs with a gap (the kill pit) + side walls + a sloped ramp (static bodies),
 *   - a dynamic box that falls and rests (gravity + solid collision + OnCollisionEnter),
 *   - a bouncy ball (restitution) and a low-friction box on the ramp (material),
 *   - a sensor pickup (overlap events + destroy-in-handler → live body removal),
 *   - a box over the gap that falls past the kill-Z (world-bounds auto-destroy),
 *   - a player capsule driven by the generic Mover (walk/jump/slide + Ground/Fly switch).
 *
 * Built in code (BuildDefaultScene); OnLoad prefers a saved *.scene.json if present, and
 * SaveScene persists on explicit request — never implicitly. Mirrors GameScene.
 */
class PhysicsSandboxScene final : public Opaax::Scene
{
public:
    PhysicsSandboxScene() : Scene("PhysicsSandboxScene") {}

    void OnLoad(Opaax::World& InWorld)    override;
    void OnUnload(Opaax::World& InWorld)  override;
    void SaveScene(Opaax::World& InWorld) override;

    void BuildDefaultScene(Opaax::World& InWorld);

private:
    // Shared sprite texture (engine asset) tinted per entity; kept resident for the scene's life.
    Opaax::TextureHandle m_SpriteTexture;
};
