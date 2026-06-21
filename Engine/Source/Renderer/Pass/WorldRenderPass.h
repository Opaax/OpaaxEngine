#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Renderer/Pass/IRenderPass.h"

namespace Opaax
{
    class CoreEngineApp;

    /**
     * @class WorldRenderPass
     *
     * Default world pass: binds the frame target, clears it, begins the batch with the active
     * world camera (RenderSubsystem), then runs every registered IWorldSystem so they issue
     * draw calls. This is the engine's pre-M7 OnRender body, relocated behind the pass interface.
     *
     * Holds the engine app by pointer (IoC — given at construction, never a global) and re-fetches
     * the active camera + scene inside Execute every frame (Lesson 17 — never cache across a swap).
     */
    class OPAAX_API WorldRenderPass final : public IRenderPass
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit WorldRenderPass(CoreEngineApp* InApp) : m_App(InApp) {}

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IRenderPass interface
    public:
        void        Execute(const RenderContext& InContext) override;
        const char* GetName() const override { return "WorldRenderPass"; }
        //~End IRenderPass interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CoreEngineApp* m_App        = nullptr;
        Vector4F       m_ClearColor = { 0.1f, 0.1f, 0.1f, 1.f };
    };
}
