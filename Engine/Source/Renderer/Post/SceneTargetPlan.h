#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES
#include "RHI/Framebuffer.h"

namespace Opaax
{
    /** The least and most render scale a world view takes: its scene target against the view's size. */
    inline constexpr float MIN_RENDER_SCALE = 0.5f;
    inline constexpr float MAX_RENDER_SCALE = 2.f;

    /** Samples per pixel of a world's target (Renderer.config): smooths the edges of quads, tiles and shapes. */
    enum class EMultisampling : Uint8
    {
        Off,
        X2,
        X4,
        X8
    };

    /** Name for logs and the value written in the config. */
    inline const char* ToString(const EMultisampling InMultisampling) noexcept
    {
        switch (InMultisampling)
        {
        case EMultisampling::Off: return "Off";
        case EMultisampling::X2:  return "X2";
        case EMultisampling::X4:  return "X4";
        case EMultisampling::X8:  return "X8";
        }
        return "Off";
    }

    /** The samples per pixel a choice asks for (1 for Off). */
    constexpr Uint32 SamplesOf(const EMultisampling InMultisampling) noexcept
    {
        switch (InMultisampling)
        {
        case EMultisampling::Off: return 1;
        case EMultisampling::X2:  return 2;
        case EMultisampling::X4:  return 4;
        case EMultisampling::X8:  return 8;
        }
        return 1;
    }

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

    /** InScale clamped to MIN_RENDER_SCALE..MAX_RENDER_SCALE; a scale that is not a number counts as 1. */
    float ClampRenderScale(float InScale) noexcept;

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

OPAAX_ENUM_VALUES(Opaax::EMultisampling, Off, X2, X4, X8)
