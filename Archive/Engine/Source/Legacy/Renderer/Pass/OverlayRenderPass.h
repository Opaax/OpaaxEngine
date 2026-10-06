#pragma once

#include "Core/EngineAPI.h"
#include "Renderer/Pass/IRenderPass.h"
#include "Renderer/Camera/ScreenSpaceCamera.h"

namespace Opaax
{
    class CoreEngineApp;

    /**
     * @class OverlayRenderPass
     *
     * Screen-space pass. Runs AFTER the world pass into the SAME target and does NOT clear,
     * so it composites on top. Begins the batch with its own ScreenSpaceCamera (sized from
     * the target each frame), then runs every registered IOverlayRenderSystem.
     *
     * Holds the engine app by pointer (IoC) and owns the screen-space camera by value.
     */
    class OPAAX_API OverlayRenderPass final : public IRenderPass
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit OverlayRenderPass(CoreEngineApp* InApp) : m_App(InApp) {}

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IRenderPass interface
    public:
        void        Execute(const RenderContext& InContext) override;
        const char* GetName() const override { return "OverlayRenderPass"; }
        //~End IRenderPass interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CoreEngineApp*    m_App = nullptr;
        ScreenSpaceCamera m_Camera;
    };
}
