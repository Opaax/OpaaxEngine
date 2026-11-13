#pragma once

#include "Core/OpaaxTypes.h"
#include "Renderer/Post/SceneTargetPlan.h"
#include "RHI/RHICapabilities.h"
#include "RHI/TextureSampling.h"

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

        /**
         * Render targets that got new storage (created, or changed size) during the last frame
         * drawn, and since the start. 0 in a steady frame: a number that stays above 0 means
         * targets are rebuilt every frame.
         */
        Uint32 TargetsRecreated      = 0;
        Uint64 TargetsAllocatedTotal = 0;

        /** Targets the lit path keeps now, for every view size in use (scene, maps, bloom levels). */
        Uint32 SceneTargets = 0;

        /** What the project and the player ask of world views (Renderer.config). */
        SceneTargetSettings SceneSettings;

        /** How each world view of the last frame was drawn, in drawing order: straight, or through a scene target. */
        TDynArray<SceneTargetPlan> WorldViews;

        /** How colour textures are read (Renderer.config's TextureFilter). */
        ETextureFilter TextureFilter = ETextureFilter::Pixel;

        /** GPU time of a recent frame, in milliseconds. Negative before the first result. */
        double GpuFrameMs = -1.0;
    };
}
