#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    /** The shape of a light. */
    enum class ELight2DType : Uint8
    {
        Point,    // all around, up to its radius
        Spot,     // a cone along the entity's rotation
        Global    // everywhere, from a direction (a sun, a moon)
    };

    /** Enum to string. */
    inline const char* ToString(const ELight2DType InType) noexcept
    {
        switch (InType)
        {
        case ELight2DType::Point:  return "Point";
        case ELight2DType::Spot:   return "Spot";
        case ELight2DType::Global: return "Global";
        }

        return "Point";
    }
}

OPAAX_ENUM_VALUES(Opaax::ELight2DType, Point, Spot, Global)

namespace Opaax
{
    // =============================================================================
    // Light2DComponent — a light at the entity. It lights the world's lit sprites when the level
    //   has an EnvironmentComponent (lighting needs the HDR path). A spot and a global light
    //   point along the entity's rotation.
    // =============================================================================
    struct Light2DComponent
    {
        ELight2DType Type = ELight2DType::Point;

        /** In screen colour, like sprite tints. */
        LinearColor Color     = { 1.f, 1.f, 1.f, 1.f };
        float       Intensity = 1.f;

        /** Point and spot: no light beyond, world units. */
        float Radius = 400.f;

        /** Point and spot: how the light fades to its radius (1 linear, 2 quadratic, ...). */
        float Falloff = 2.f;

        /** Spot: the cone's full angle, degrees. */
        float ConeAngle = 60.f;

        /** Spot: 0 is a hard cone edge, 1 fades from the centre. */
        float ConeSoftness = 0.3f;

        /** Point and spot: how high above the scene, world units. Low lights graze normal maps. */
        float Height = 100.f;

        /** Global: the light's angle above the scene, degrees (90 is straight down). */
        float Elevation = 45.f;

        /** Point and spot: ShadowCaster2Ds block it. A view shadows its 16 strongest such lights. */
        bool bCastShadows = false;

        /** 0: hard shadow edges. Higher: edges soften with the distance behind the caster. */
        float ShadowSoftness = 0.5f;

        /** 0: no shadow, 1: no light at all in the shadow. */
        float ShadowStrength = 1.f;

        bool bEnabled = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Light2DComponent, Type, Color, Intensity, Radius, Falloff,
                                                    ConeAngle, ConeSoftness, Height, Elevation, bCastShadows,
                                                    ShadowSoftness, ShadowStrength, bEnabled)

        OPAAX_PROPERTIES(Light2DComponent,
                         OPAAX_PROP(Type).SetTooltip("Point lights all around, Spot a cone along the entity's\n"
                                                     "rotation, Global everywhere (a sun)."),
                         OPAAX_PROP(Color),
                         OPAAX_PROP(Intensity).SetRange(0.f, 50.f).SetDragStep(0.05f)
                                              .SetTooltip("Above 1 the light can saturate colours."),
                         OPAAX_PROP(Radius).SetRange(1.f, 10000.f).SetTooltip("No light beyond. Point and Spot."),
                         OPAAX_PROP(Falloff).SetRange(0.1f, 8.f).SetDragStep(0.05f)
                                            .SetTooltip("How it fades to the radius: 1 linear, 2 quadratic."),
                         OPAAX_PROP(ConeAngle).SetRange(1.f, 360.f).SetTooltip("Spot: the cone's full angle."),
                         OPAAX_PROP(ConeSoftness).SetRange(0.f, 1.f).SetDragStep(0.01f)
                                                 .SetTooltip("Spot: 0 hard edge, 1 fades from the centre."),
                         OPAAX_PROP(Height).SetRange(1.f, 10000.f)
                                           .SetTooltip("Height above the scene. Low lights graze normal maps."),
                         OPAAX_PROP(Elevation).SetRange(1.f, 90.f)
                                              .SetTooltip("Global: the light's angle above the scene (90 = from above)."),
                         OPAAX_PROP(bCastShadows).SetTooltip("Point and Spot: ShadowCaster2Ds block this light.\n"
                                                             "A view shadows its 16 strongest shadowed lights."),
                         OPAAX_PROP(ShadowSoftness).SetRange(0.f, 4.f).SetDragStep(0.01f)
                                                   .SetTooltip("0: hard edges. Higher: edges soften with the\n"
                                                               "distance behind the caster."),
                         OPAAX_PROP(ShadowStrength).SetRange(0.f, 1.f).SetDragStep(0.01f)
                                                   .SetTooltip("0: no shadow, 1: no light at all in the shadow."),
                         OPAAX_PROP(bEnabled))
    };
}
