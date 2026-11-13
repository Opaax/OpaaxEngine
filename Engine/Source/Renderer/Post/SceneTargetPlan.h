#pragma once

#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    /** The least and most render scale a world view takes: its scene target against the view's size. */
    inline constexpr float MIN_RENDER_SCALE = 0.5f;
    inline constexpr float MAX_RENDER_SCALE = 2.f;

    /** What the project and the player ask of world views (Renderer.config), as the plan reads it. */
    struct SceneTargetSettings
    {
        /** Samples per pixel of the world's target. 1: no multisampling. */
        Uint32 Samples = 1;

        /** Smooth the finished picture's edges (FXAA) before the overlays and the UI. */
        bool bFXAA = false;

        /** The world's target against its view, MIN_RENDER_SCALE to MAX_RENDER_SCALE. */
        float RenderScale = 1.f;
    };

    /** How one world view is drawn: straight into its target, or through a scene target. */
    struct SceneTargetPlan
    {
        /** False: the world is drawn straight into the view's target, and the rest is unused. */
        bool bSceneTarget = false;

        /** The scene target the world is drawn into. */
        Uint32         Width   = 0;
        Uint32         Height  = 0;
        ETextureFormat Format  = ETextureFormat::RGBA8;
        Uint32         Samples = 1;

        /** Lit: exposure, tonemap and gamma after it. Unlit: copied as it is. */
        bool bLit = false;

        /** FXAA reads a screen-colour target of the view's size, filled from the scene, and writes the view's. */
        bool bFXAATarget = false;
    };

    /**
     * How a world view of InWidth x InHeight is drawn. A scene target is used when the world is lit,
     * or when a setting asks for one (samples above 1, FXAA, a scale other than 1); RGBA16F when lit,
     * RGBA8 otherwise. Samples are capped by InMaxSamples (the device's most); the scale is clamped,
     * and each side keeps 1 pixel at least.
     */
    SceneTargetPlan PlanSceneTarget(Uint32 InWidth, Uint32 InHeight, bool bInLit,
                                    const SceneTargetSettings& InSettings, Uint32 InMaxSamples);
}
