#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Profiling/FrameProfiler.h"

namespace Opaax
{
    /**
     * @struct FrameStats
     *
     * ONE snapshot of what the last COMPLETE frame cost, owned by the stats app service and read
     * through IStatsService::GetFrameStats().
     *
     * Every field describes the SAME frame — the one before the current host iteration — and that
     * consistency is the whole point. The Stats panel draws in the middle of the wall-clock frame,
     * after Engine::Loop and before Present, so "the frame so far" would report a total its own rows
     * could not add up to. The service publishes the whole snapshot at the host loop's top instead,
     * where the previous frame is finished.
     *
     * Nothing here survives to the next frame (F4's immediate-mode doctrine): this is a measurement,
     * not a history. Smoothing and graphs are the reader's business.
     *
     * Header-only value type, no OPAAX_API (I6).
     */
    struct FrameStats
    {
        /** The whole frame — poll, tick, UI pass and present — host-loop top to host-loop top. */
        double FrameMs = 0.0;

        /**
         * What the GPU spent on a recent frame, or NEGATIVE when there is no reading (④ S3).
         *
         * A DURATION, like FrameMs beside it — Core learns nothing about GPUs, only that a frame has
         * a second clock. Deliberately NOT a scope in the tree: GPU work runs alongside the CPU
         * rather than inside it, so a top-level row would be double-counted against the frame total
         * and drive the "Other" remainder negative.
         *
         * It is 1-2 frames old by construction (see IRHIDevice::GetLastGpuFrameTimeMs), so it does
         * not line up with this snapshot's scopes as exactly as everything else here does.
         */
        double GpuMs = -1.0;

        /**
         * The frame's named scopes, in pre-order. Publishes itself — it holds the two buffers.
         *
         * There is no separate fixed-step count: the "FixedUpdate" scope sits INSIDE the catch-up
         * loop, so its Calls is the step count and its Milliseconds is the total. One mechanism.
         */
        FrameProfiler Profiler;
    };
}
