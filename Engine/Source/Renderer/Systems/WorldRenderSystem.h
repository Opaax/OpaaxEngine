#pragma once

#include "Core/EngineAPI.h"
#include "World/IWorldSystem.h"

namespace Opaax
{
    /**
     * @class WorldRenderSystem
     *
     * Stateless render system — iterates entities with TransformComponent +
     * SpriteComponent and issues Renderer2D::DrawSprite calls. Must be invoked
     * between Renderer2D::Begin() and Renderer2D::End().
     */
    class OPAAX_API WorldRenderSystem final : public IWorldSystem
    {
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IWorldSystem Interface
        void OnRender(World& InWorld, const RenderContext& InContext) override;
        //~End IWorldSystem Interface
    };
}
