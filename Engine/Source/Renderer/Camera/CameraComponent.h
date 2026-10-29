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
    //   The view stays upright unless bRotateView: then it rolls with the entity's world rotation.
    // =============================================================================
    struct CameraComponent
    {
        /** The range OrthoSize is kept in: a zero or negative size would break the projection. */
        static constexpr float MIN_ORTHO_SIZE = 1.f;
        static constexpr float MAX_ORTHO_SIZE = 100000.f;

        /** Vertical half-extent in world units. Smaller = zoomed in. */
        float OrthoSize = 300.f;

        /** With several cameras, the highest frames the world (among equals, the first found). */
        Int32 Priority = 0;

        /** Roll the view with the entity's world rotation. Off, a camera under a turning body stays upright. */
        bool bRotateView = false;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraComponent, OrthoSize, Priority, bRotateView)

        // Range: a zero or negative size would break the projection.
        OPAAX_PROPERTIES(CameraComponent,
                         OPAAX_PROP(OrthoSize).SetRange(MIN_ORTHO_SIZE, MAX_ORTHO_SIZE),
                         OPAAX_PROP(Priority).SetDragStep(1.f).SetTooltip("With several cameras, the highest frames the world."),
                         OPAAX_PROP(bRotateView).SetTooltip("Roll the view with the camera's rotation.\n"
                                                            "Off: the view stays upright, even under a turning body."))
    };
}
