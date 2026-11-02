#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // CameraZoneComponent — a part of a level that sets how a rigged camera frames it (a room): a
    //   rectangle HalfExtent each side of the entity (times its scale), the OrthoSize the camera eases
    //   to there, and whether the view is kept inside it. A CameraZoneStage on a camera applies the
    //   highest-priority zone holding its target, blending over the zone's BlendTime.
    // =============================================================================
    struct CameraZoneComponent
    {
        /** Half its width and height, from the entity's position, times its scale. */
        Vector2F HalfExtent = { 400.f, 300.f };

        /** The camera's OrthoSize here; 0 keeps the size it has. */
        float OrthoSize = 0.f;

        /** The whole view is kept inside the zone (centred on it when wider). */
        bool bConfine = true;

        /** Among zones holding the target, the highest applies (among equals, the first found). */
        Int32 Priority = 0;

        /** Seconds the camera takes to blend in when the zone starts to apply. */
        float BlendTime = 0.5f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraZoneComponent, HalfExtent, OrthoSize, bConfine, Priority,
                                                    BlendTime)

        OPAAX_PROPERTIES(CameraZoneComponent,
                         OPAAX_PROP(HalfExtent).SetTooltip("Half its width and height, from the entity, times its "
                                                           "scale."),
                         OPAAX_PROP(OrthoSize).SetRange(0.f, 100000.f).SetTooltip("The camera's OrthoSize here.\n"
                                                                                  "0: keeps the size it has."),
                         OPAAX_PROP(bConfine).SetTooltip("Keeps the whole view inside the zone."),
                         OPAAX_PROP(Priority).SetTooltip("Among zones holding the target, the highest applies."),
                         OPAAX_PROP(BlendTime).SetRange(0.f, 10.f).SetTooltip("Seconds the camera takes to blend in "
                                                                              "when the zone starts to apply."))
    };
}
