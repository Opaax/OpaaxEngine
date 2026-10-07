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
    /** How HDR colour is brought into the screen's range. */
    enum class ETonemapper : Uint8
    {
        None,       // clipped: what a scene without lighting looks like
        Reinhard,   // soft roll-off, a little flat
        ACES        // filmic contrast and saturation
    };

    /** Enum to string. */
    inline const char* ToString(const ETonemapper InTonemapper) noexcept
    {
        switch (InTonemapper)
        {
        case ETonemapper::None:     return "None";
        case ETonemapper::Reinhard: return "Reinhard";
        case ETonemapper::ACES:     return "ACES";
        }

        return "None";
    }
}

OPAAX_ENUM_VALUES(Opaax::ETonemapper, None, Reinhard, ACES)

namespace Opaax
{
    // =============================================================================
    // EnvironmentComponent — how a world's picture is made. Put one on any entity of the level
    //   (the first found is used). With it, the world is drawn in HDR: colour in linear space,
    //   lit by its Light2Ds and the ambient light (darkened near casters with ambient occlusion),
    //   then exposure and tonemapping. Without it, the world is drawn as before, directly, and
    //   lights are ignored.
    // =============================================================================
    struct EnvironmentComponent
    {
        /** The light everything lit gets, in screen colour. White at 1 is the unlit look: dim it for night. */
        LinearColor AmbientColor     = { 1.f, 1.f, 1.f, 1.f };
        float       AmbientIntensity = 1.f;

        /** Darkens the ambient light near ShadowCaster2Ds: contact shadows that need no light. */
        bool  bAmbientOcclusion = false;

        /** How far from a caster the darkening reaches, world units. */
        float AORadius = 48.f;

        /** How dark it gets against a caster: 0 none, 1 no ambient light at all. */
        float AOStrength = 0.6f;

        /** Brightness in stops: +1 doubles, -1 halves. */
        float Exposure = 0.f;

        ETonemapper Tonemapper = ETonemapper::ACES;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EnvironmentComponent, AmbientColor, AmbientIntensity,
                                                    bAmbientOcclusion, AORadius, AOStrength, Exposure, Tonemapper)

        OPAAX_PROPERTIES(EnvironmentComponent,
                         OPAAX_PROP(AmbientColor).SetTooltip("The light everything lit gets, without any Light2D."),
                         OPAAX_PROP(AmbientIntensity).SetRange(0.f, 10.f).SetDragStep(0.01f)
                                                     .SetTooltip("1 with a white colour: sprites as drawn.\n"
                                                                 "Lower it and the lights show."),
                         OPAAX_PROP(bAmbientOcclusion).SetTooltip("Darkens the ambient light near ShadowCaster2Ds:\n"
                                                                  "contact shadows that need no light."),
                         OPAAX_PROP(AORadius).SetRange(1.f, 1000.f)
                                             .SetTooltip("How far from a caster the darkening reaches, world units."),
                         OPAAX_PROP(AOStrength).SetRange(0.f, 1.f).SetDragStep(0.01f)
                                               .SetTooltip("How dark it gets against a caster:\n"
                                                           "0 none, 1 no ambient light at all."),
                         OPAAX_PROP(Exposure).SetRange(-8.f, 8.f)
                                             .SetTooltip("Brightness in stops: +1 doubles the light, -1 halves it."),
                         OPAAX_PROP(Tonemapper).SetTooltip("How bright colours are brought into the screen's range.\n"
                                                           "None clips them; ACES gives a filmic look."))
    };
}
