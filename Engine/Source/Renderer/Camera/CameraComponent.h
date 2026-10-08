#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // CameraComponent — how the world is framed. CameraManager turns it into the world's CameraView.
    //   OrthoSize is the vertical half-extent in world units (a bigger window scales the view).
    //   The position is the entity's world position (a camera parented to the player follows it).
    //   With several cameras, the highest Priority frames the world: raise one to switch to it.
    // =============================================================================
    struct CameraComponent
    {
        /** Vertical half-extent in world units. Smaller = zoomed in. */
        float OrthoSize = 300.f;

        /** With several cameras, the highest frames the world (among equals, the first found). */
        Int32 Priority = 0;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraComponent, OrthoSize, Priority)

        // Range: a zero or negative size would break the projection.
        OPAAX_PROPERTIES(CameraComponent,
                         OPAAX_PROP(OrthoSize).SetRange(1.f, 100000.f),
                         OPAAX_PROP(Priority).SetDragStep(1.f).SetTooltip("With several cameras, the highest frames the world."))
    };
}
