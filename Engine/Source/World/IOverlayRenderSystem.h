#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    class World;
    struct RenderContext;

    /**
     * @class IOverlayRenderSystem
     *
     * Screen-space sibling of IWorldSystem. Implementers issue draw calls in pixel space
     * (HUD, score, debug text/markers) that stay locked to the screen regardless of the
     * world camera. The OverlayRenderPass calls OnRenderOverlay on every registered system
     * between Renderer2D::Begin(screenSpaceCamera) and End() — systems never own Begin/End
     * nor the camera, they only submit draws.
     *
     * Register — from a game's CoreEngineApp::OnStartup override:
     *   GetSubsystem<RenderSubsystem>()->RegisterOverlaySystem(MakeUnique<MyHudSystem>());
     *
     * The overlay pass runs AFTER the world pass into the same target and does not clear,
     * so overlay draws composite on top of the world.
     */
    class OPAAX_API IOverlayRenderSystem
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IOverlayRenderSystem() = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        virtual void OnRenderOverlay(World& InWorld, const RenderContext& InContext) = 0;
    };
}
