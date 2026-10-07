#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    struct Light2DComponent;

    /** The most lights one view is lit by. With more, the strongest win. */
    inline constexpr Uint32 MAX_LIGHTS_2D = 32;

    /** Light2D type codes as the shader reads them. */
    inline constexpr float LIGHT_CODE_POINT  = 0.f;
    inline constexpr float LIGHT_CODE_SPOT   = 1.f;
    inline constexpr float LIGHT_CODE_GLOBAL = 2.f;

    /** The most lights one view shadows: the shadow map's rows. With more, the strongest win. */
    inline constexpr Uint32 MAX_SHADOWED_LIGHTS_2D = 16;

    /** Directions per light in the shadow map (its width). */
    inline constexpr Uint32 SHADOW_MAP_ANGLES_2D = 1024;

    /** The occlusion map spans the view and the shadowed lights' reach, at most this many times the view's size. */
    inline constexpr float OCCLUSION_MAX_SCALE_2D = 3.f;

    /** The occlusion map's largest side, pixels: past it, it gets coarser rather than bigger. */
    inline constexpr Uint32 OCCLUSION_MAX_SIZE_2D = 4096;

    /** The ambient occlusion map has one texel per this many occlusion map texels, each way. */
    inline constexpr Uint32 AO_DOWNSAMPLE_2D = 4;

    // =============================================================================
    // LightsBlock2D — the LightsUBO block of Sprite.glsl (std140: every member a vec4).
    //   Colours are linear (decoded from screen colour) and already multiplied by intensity.
    // =============================================================================
    struct LightsBlock2D
    {
        Vector4F Ambient   = { 1.f, 1.f, 1.f, 0.f };   // rgb: ambient light, w: light count
        Vector4F AORect    = { 0.f, 0.f, 1.f, 1.f };   // xy: the ambient occlusion map's world min, zw: its world size
        Vector4F AOParams  = { 0.f, 0.f, 0.f, 0.f };   // x: strength (0: no ambient occlusion)
        Vector4F Position[MAX_LIGHTS_2D]{};            // xy: position, z: radius, w: type code
        Vector4F Color[MAX_LIGHTS_2D]{};             // rgb: colour * intensity, w: falloff exponent
        Vector4F Direction[MAX_LIGHTS_2D]{};         // xy: where it points, z: cos(outer half-angle), w: cos(inner)

        // x: height (point, spot) or sin(elevation) (global), y: shadow map row (-1: none),
        // z: shadow softness, w: shadow strength
        Vector4F Params[MAX_LIGHTS_2D]{};

        Uint32 GetCount() const noexcept { return static_cast<Uint32>(Ambient.w); }
    };

    static_assert(sizeof(LightsBlock2D) == sizeof(Vector4F) * (3 + 4 * MAX_LIGHTS_2D), "std140: vec4 members only");

    // =============================================================================
    // ShadowBlock2D — the ShadowUBO block of Shadow2D.glsl, the pass that fills the shadow map:
    //   for each row, how far the light travels in each direction before a caster stops it.
    // =============================================================================
    struct ShadowBlock2D
    {
        Vector4F OcclusionRect = { 0.f, 0.f, 1.f, 1.f };   // xy: the occlusion map's world min, zw: its world size
        Vector4F Info          = { 0.f, static_cast<float>(SHADOW_MAP_ANGLES_2D), 0.f, 0.f };  // x: rows, y: angles
        Vector4F Light[MAX_SHADOWED_LIGHTS_2D]{};          // xy: position, z: radius

        Uint32 GetCount() const noexcept { return static_cast<Uint32>(Info.x); }
    };

    static_assert(sizeof(ShadowBlock2D) == sizeof(Vector4F) * (2 + MAX_SHADOWED_LIGHTS_2D), "std140: vec4 members only");

    /** Where the casters are drawn for a view's shadows: a world area and its size in pixels. */
    struct OcclusionLayout2D
    {
        Bounds2D Bounds;
        Uint32   Width  = 1;
        Uint32   Height = 1;
    };

    /** One light of a world, where it is drawn this frame. */
    struct Light2DInstance
    {
        const Light2DComponent* Light = nullptr;
        Vector2F                Position = { 0.f, 0.f };
        float                   RotationDegrees = 0.f;
    };

    /** A screen (sRGB) colour channel as linear light. */
    float ScreenToLinear(float InChannel) noexcept;

    /**
     * Fills OutBlock with the ambient light and the lights that reach InView: enabled, with some
     * intensity, a global light always, a point or spot light when its radius touches the view.
     * With more than MAX_LIGHTS_2D, the strongest (intensity and reach, nearest the view) are kept;
     * equal ones keep their order. Kept point and spot lights that cast shadows get a shadow map
     * row, the strongest first, MAX_SHADOWED_LIGHTS_2D at most.
     * @param InAmbientColor     Screen colour of the light everything gets
     * @param InAmbientIntensity Multiplies it (linear)
     * @return Lights that reach the view (before the limit)
     */
    Uint32 PackLights2D(const TDynArray<Light2DInstance>& InLights, const Bounds2D& InView,
                        const Vector3F& InAmbientColor, float InAmbientIntensity, LightsBlock2D& OutBlock);

    /** Turns every light's shadow off (no shadow map this frame). */
    void ClearShadowRows2D(LightsBlock2D& InOutBlock) noexcept;

    /**
     * Lists the shadowed lights of InLights by row, for the shadow map pass (the occlusion rect is
     * left to the caller).
     * @return Rows used
     */
    Uint32 BuildShadowBlock2D(const LightsBlock2D& InLights, ShadowBlock2D& OutBlock) noexcept;

    /**
     * The area the casters are drawn in: the view and the shadowed lights' reach, at most
     * OCCLUSION_MAX_SCALE_2D times the view's size, at InPixelsPerUnit (less when the map would pass
     * OCCLUSION_MAX_SIZE_2D). The bounds are fitted to whole pixels.
     */
    OcclusionLayout2D MakeOcclusionLayout2D(const Bounds2D& InView, float InPixelsPerUnit,
                                            const ShadowBlock2D& InShadows) noexcept;

    /** The ambient occlusion map's size along one side, from the occlusion map's. */
    Uint32 AmbientOcclusionExtent2D(Uint32 InOcclusionExtent) noexcept;

    /**
     * The blur that turns the casters' coverage into ambient occlusion, in ambient occlusion map
     * texels: half of InRadius (world units), so the darkening fades out by InRadius.
     */
    float AmbientOcclusionSigma2D(float InRadius, const OcclusionLayout2D& InLayout) noexcept;

    /** Turns ambient occlusion on in InOutBlock: its map covers InLayout's area. */
    void PackAmbientOcclusion2D(LightsBlock2D& InOutBlock, const OcclusionLayout2D& InLayout, float InStrength) noexcept;
}
