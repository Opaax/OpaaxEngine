#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // QuadComponent — a solid coloured rectangle (size, colour), centred on the entity. For an image,
    //   use a SpriteComponent.
    // =============================================================================
    struct QuadComponent
    {
        Vector2F    Size     = { 50.f, 50.f };
        LinearColor Color    = { 1.f, 1.f, 1.f, 1.f };

        // Serialization (see ComponentConcept.hpp).
        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(QuadComponent, Size, Color)

        // Editable in the Inspector.
        OPAAX_PROPERTIES(QuadComponent,
                         OPAAX_PROP(Size).SetRange(1.f, 4096.f),
                         OPAAX_PROP(Color))
    };
}
