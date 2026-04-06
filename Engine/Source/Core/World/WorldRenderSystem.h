#pragma once

#pragma once

#include "World.h"
#include "Core/EngineAPI.h"

namespace Opaax::ECS
{
    /**
     * @class WorldRenderSystem
     *
     * Stateless — reads Transform + Sprite from the World, calls Renderer2D.
     * Called by CoreEngineApp::OnRender() (or future SceneManager).
     *
     * Camera is passed by ref each frame — the system does not own it.
     */
    class OPAAX_API WorldRenderSystem
    {
        // =============================================================================
        // Static
        // =============================================================================
    public:
        // Renders all entities with TransformComponent + SpriteComponent.
        // Must be called between Renderer2D::Begin() and Renderer2D::End().
        static void Render(World& InWorld);
    };

} // namespace Opaax::ECS