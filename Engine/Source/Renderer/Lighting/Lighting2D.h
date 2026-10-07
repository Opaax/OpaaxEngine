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

    // =============================================================================
    // LightsBlock2D — the LightsUBO block of Sprite.glsl (std140: every member a vec4).
    //   Colours are linear (decoded from screen colour) and already multiplied by intensity.
    // =============================================================================
    struct LightsBlock2D
    {
        Vector4F Ambient = { 1.f, 1.f, 1.f, 0.f };   // rgb: ambient light, w: light count
        Vector4F Position[MAX_LIGHTS_2D]{};          // xy: position, z: radius, w: type code
        Vector4F Color[MAX_LIGHTS_2D]{};             // rgb: colour * intensity, w: falloff exponent
        Vector4F Direction[MAX_LIGHTS_2D]{};         // xy: where it points, z: cos(outer half-angle), w: cos(inner)
        Vector4F Params[MAX_LIGHTS_2D]{};            // x: height (point, spot) or sin(elevation) (global), y: shadow row (-1 none)

        Uint32 GetCount() const noexcept { return static_cast<Uint32>(Ambient.w); }
    };

    static_assert(sizeof(LightsBlock2D) == sizeof(Vector4F) * (1 + 4 * MAX_LIGHTS_2D), "std140: vec4 members only");

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
     * equal ones keep their order.
     * @param InAmbientColor     Screen colour of the light everything gets
     * @param InAmbientIntensity Multiplies it (linear)
     * @return Lights that reach the view (before the limit)
     */
    Uint32 PackLights2D(const TDynArray<Light2DInstance>& InLights, const Bounds2D& InView,
                        const Vector3F& InAmbientColor, float InAmbientIntensity, LightsBlock2D& OutBlock);
}
