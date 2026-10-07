#pragma once

#include <nlohmann/json.hpp>

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
    //   then exposure and tonemapping. Without it, the world is drawn as before, directly.
    // =============================================================================
    struct EnvironmentComponent
    {
        /** Brightness in stops: +1 doubles, -1 halves. */
        float Exposure = 0.f;

        ETonemapper Tonemapper = ETonemapper::ACES;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EnvironmentComponent, Exposure, Tonemapper)

        OPAAX_PROPERTIES(EnvironmentComponent,
                         OPAAX_PROP(Exposure).SetRange(-8.f, 8.f)
                                             .SetTooltip("Brightness in stops: +1 doubles the light, -1 halves it."),
                         OPAAX_PROP(Tonemapper).SetTooltip("How bright colours are brought into the screen's range.\n"
                                                           "None clips them; ACES gives a filmic look."))
    };
}
