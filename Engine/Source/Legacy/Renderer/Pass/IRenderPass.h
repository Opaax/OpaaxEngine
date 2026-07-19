#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    struct RenderContext;

    /**
     * @interface IRenderPass
     *
     * One ordered stage of the frame. The engine owns a RenderPipeline (an ordered list
     * of passes) and calls Execute on each, in order, once per frame. A pass decides its
     * own render target, camera, clear policy, and what it submits — Execute is handed the
     * shared RenderContext (final target + interpolation alpha).
     *
     * Adding a render feature = writing a pass class + registering it on the pipeline.
     * Built-ins: WorldRenderPass (world camera), OverlayRenderPass (screen space).
     *
     * Submission stays on the GL thread; a pass may use the Job system for off-thread
     * prep but must issue draw calls from Execute.
     */
    class OPAAX_API IRenderPass
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IRenderPass() = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        virtual void        Execute(const RenderContext& InContext) = 0;
        virtual const char* GetName() const                         = 0;
    };
}
