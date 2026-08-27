#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // CameraComponent — the AUTHORED camera: what an entity says about how its world
    //   should be framed. CameraManager resolves it into the world's CameraView every
    //   frame; the renderer turns that into matrices (Renderer/CameraView.h).
    //
    //   OrthoSize is the vertical HALF-EXTENT in world units, so a bigger window scales
    //   the view instead of showing more playfield. The default is the frame the engine
    //   drew before cameras existed — a map that gains a camera does not jump.
    //
    //   WHERE it looks from is its entity's TransformComponent — so a camera that must sit
    //   somewhere other than the thing it follows is its OWN entity, as it is in every other
    //   engine. Priority is deliberately absent: with several cameras the FIRST wins and
    //   CameraManager warns, because a field nothing reads is a spec (X5).
    // =============================================================================
    struct CameraComponent
    {
        /** Vertical half-extent in world units. Smaller = zoomed in. */
        float    OrthoSize = 300.f;

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference: the
        // plain macro reads every field with at(), which THROWS on a missing key.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraComponent, OrthoSize)

        // The field type already has a built-in drawer, so the Inspector needs no camera code.
        // The range is what stops a zero or negative size — which would collapse the projection.
        OPAAX_PROPERTIES(CameraComponent,
                         OPAAX_PROP(OrthoSize).SetRange(1.f, 100000.f))
    };
}
