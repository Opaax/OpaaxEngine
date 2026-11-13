#pragma once

#include "Core/OpaaxTypes.h"
#include "RHI/RHICapabilities.h"

namespace Opaax
{
    // =============================================================================
    // RendererInfo — what the renderer is using right now, as plain data: for a person (the
    //   console's renderer.info), a script or a test. What is in use, not what was asked for.
    // =============================================================================
    struct RendererInfo
    {
        /** False when there is no render device (a headless run): the rest is then empty. */
        bool bValid = false;

        /** The device and its driver: names and limits. */
        RHICapabilities Device;

        /** The window's drawing surface, in pixels. */
        Uint32 BackbufferWidth  = 0;
        Uint32 BackbufferHeight = 0;

        /** GPU time of a recent frame, in milliseconds. Negative before the first result. */
        double GpuFrameMs = -1.0;
    };
}
