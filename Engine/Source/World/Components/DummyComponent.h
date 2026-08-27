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
    // DummyComponent — temporary placeholder so a World can be populated and drawn
    //   end-to-end before the real component set exists. Carries just enough to render
    //   a solid quad (position, size, color). Not meant to survive past bring-up.
    // =============================================================================
    struct DummyComponent
    {
        Vector2F    Size     = { 50.f, 50.f };
        LinearColor Color    = { 1.f, 1.f, 1.f, 1.f };

        // Satisfies CComponent (World/Components/ComponentConcept.hpp) — generates the
        // to_json/from_json pair ComponentRegistry needs. This one macro is the entire cost
        // of making a component serializable; the glm members resolve through MathsJson.hpp.
        //
        // _WITH_DEFAULT is the required variant, not a preference: the plain macro reads every
        // field with at(), which THROWS on a missing key, so adding a field here would refuse
        // to open every map saved before it existed — at boot, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DummyComponent, Size, Color)

        // What the Inspector draws, with no drawer written for it. Color needs no facet — its TYPE
        // says it is a colour. Size states the clamp the hand-written drawer used to carry.
        OPAAX_PROPERTIES(DummyComponent,
                         OPAAX_PROP(Size).SetRange(1.f, 4096.f),
                         OPAAX_PROP(Color))
    };
}
