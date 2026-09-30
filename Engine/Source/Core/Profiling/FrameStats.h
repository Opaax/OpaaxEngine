#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Profiling/FrameProfiler.h"

namespace Opaax
{
    /**
     * Stats of the last complete frame. Read through Profiler::GetFrameStats().
     */
    struct FrameStats
    {
        /** Whole frame time: poll, tick, UI and present. */
        double FrameMs = 0.0;

        /**
         * GPU time of a recent frame (1-2 frames old), or negative when unavailable.
         */
        double GpuMs = -1.0;

        /**
         * The frame's named scopes, in pre-order. The "FixedUpdate" scope's Calls is the step count.
         */
        FrameProfiler Profiler;
    };
}
