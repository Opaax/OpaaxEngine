#pragma once

#include "Core/OpaaxTypes.h"   // Uint32

namespace Opaax
{
    // =============================================================================
    // Render stats
    // =============================================================================

    /**
     * Per-frame renderer counters, accumulated across every Renderer2D Begin/End pass in a frame and
     * surfaced one frame late (so the stats overlay's own draws never perturb the numbers it shows).
     * Read via Renderer2D::GetStats(); rolled over by Renderer2D::NewFrame().
     */
    struct RenderStats
    {
        Uint32 Quads            = 0;   // quads submitted this frame
        Uint32 DrawCalls        = 0;   // == Batches (one indexed draw per batch)
        Uint32 Batches          = 0;   // batches emitted (split on quad/slot pressure)
        Uint32 PeakTextureSlots = 0;   // most distinct slots (incl. white) used by a single batch
        Uint32 RingHighWater    = 0;   // peak Vulkan descriptor-ring cursor this frame (0 on OpenGL)
        Uint32 CommandCapacity  = 0;   // persistent command-list capacity (realloc watch)
        double SortMicros       = 0.0; // total time spent in the frame-global sort, microseconds
    };
}
