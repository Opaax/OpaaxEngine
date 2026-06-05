#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Pass/IRenderPass.h"

namespace Opaax
{
    class IRenderTarget;

    /**
     * @class RenderPipeline
     *
     * Ordered list of IRenderPass. Owned by RenderSubsystem. Execute() runs every pass in
     * registration order against a shared RenderContext built from the active render target
     * and the frame's interpolation alpha. Dumb executor by design — passes hold their own
     * dependencies (camera, world access) so the pipeline stays decoupled from gameplay.
     *
     * IRenderPass is included (not forward-declared) so the UniquePtr element type is complete:
     * keeps dtor/move implicit and the member self-contained.
     */
    class OPAAX_API RenderPipeline
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        RenderPipeline()  = default;
        ~RenderPipeline() = default;

        // =============================================================================
        // Copy - delete / Move - default (move-only: holds UniquePtr passes)
        // =============================================================================
        RenderPipeline(const RenderPipeline&)            = delete;
        RenderPipeline& operator=(const RenderPipeline&) = delete;
        RenderPipeline(RenderPipeline&&)                 noexcept = default;
        RenderPipeline& operator=(RenderPipeline&&)      noexcept = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        // Append a pass to the end of the execution order.
        void AddPass(UniquePtr<IRenderPass> InPass);

        // Run every pass in order. InTarget is the frame's final render target.
        void Execute(IRenderTarget& InTarget, double InAlpha);

        // Drop all passes (subsystem shutdown).
        void Clear();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<UniquePtr<IRenderPass>> m_Passes;
    };
}
