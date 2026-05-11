#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    class World;
    struct RenderContext;

    /**
     * @class IWorldSystem
     *
     * Render-side system that iterates a World and issues draw calls.
     * Concrete impls register through TPolymorphicList<IWorldSystem>;
     * the engine dispatches them between Renderer2D::Begin() and End()
     * inside CoreEngineApp::OnRender.
     *
     * Register at startup:
     *   TPolymorphicList<IWorldSystem>::Register(MakeUnique<MyRenderSystem>());
     */
    class OPAAX_API IWorldSystem
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IWorldSystem() = default;

        // =============================================================================
        // Function
        // =============================================================================

        virtual void OnRender(World& InWorld, const RenderContext& InContext) = 0;
    };
}
