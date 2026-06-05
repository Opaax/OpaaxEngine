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
     * CoreEngineApp::OnRender dispatches every registered system between
     * Renderer2D::Begin() and End().
     *
     * Define a system:
     *   class MyOverlaySystem final : public IWorldSystem
     *   {
     *   public:
     *       void OnRender(World& InWorld, const RenderContext& InContext) override;
     *   };
     *
     * Register — typically from a game's CoreEngineApp::OnInitialize override:
     *   TPolymorphicList<IWorldSystem>::Register(MakeUnique<MyOverlaySystem>());
     *
     * Order. The engine registers WorldRenderSystem inside CoreEngineApp::Initialize
     * *before* OnInitialize runs, so game-registered systems draw on top of the
     * default world render in registration order. Systems do not own Begin/End nor the
     * camera — the owning WorldRenderPass calls Renderer2D::Begin/End with the active
     * camera; systems only issue draw calls between them.
     *
     * RenderContext supplies the render target and the physics-step interpolation alpha;
     * extend it only when a concrete system needs more.
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
