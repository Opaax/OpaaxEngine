#pragma once

#include "Core/CoreEngineApp.h"

class BulletStormRenderSystem;

/**
 * @class RendererStressDemoApp
 *
 * Standalone consumer project (sibling to Game/, SimpleShmup/, PhysicsDemo/) that validates M11 —
 * Renderer Capacity & Render Stats. A single procedural render system (BulletStormRenderSystem) draws
 * 5000+ quads across 3 layers and 24 distinct textures in a deliberately interleaved submission order,
 * plus a marker at the top layer/order that must always stay on top — which the pre-M11 per-flush sort
 * provably could not guarantee past one batch. Enable render.stats in engine.config.json to see the
 * draw-call / batch / quad / ring-high-water overlay.
 */
class RendererStressDemoApp : public Opaax::CoreEngineApp
{
public:
    RendererStressDemoApp(int InArgc, char** InArgv);

protected:
    void OnInitialize() override;
    void OnStartup()    override;
private:
};
