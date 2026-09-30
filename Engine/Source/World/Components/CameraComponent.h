#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // CameraComponent — how the world is framed. CameraManager turns it into the world's CameraView.
    //   OrthoSize is the vertical half-extent in world units (a bigger window scales the view).
    //   The position is the entity's Transform. With several cameras, the first wins.
    // =============================================================================
    struct CameraComponent
    {
        /** Vertical half-extent in world units. Smaller = zoomed in. */
        float    OrthoSize = 300.f;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraComponent, OrthoSize)

        // Range: a zero or negative size would break the projection.
        OPAAX_PROPERTIES(CameraComponent,
                         OPAAX_PROP(OrthoSize).SetRange(1.f, 100000.f))
    };
}
