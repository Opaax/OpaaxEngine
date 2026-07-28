#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "World/Components/ComponentBase.h"

namespace Opaax
{
    // =============================================================================
    // DummyComponent — temporary placeholder so a World can be populated and drawn
    //   end-to-end before the real component set exists. Carries just enough to render
    //   a solid quad (position, size, color). Not meant to survive past bring-up.
    // =============================================================================
    struct DummyComponent : ComponentBase
    {
        Vector2F Position = { 0.f, 0.f };
        Vector2F Size     = { 50.f, 50.f };
        Vector4F Color    = { 1.f, 1.f, 1.f, 1.f };

        // Satisfies CComponent (World/Components/ComponentConcept.hpp) — generates the
        // to_json/from_json pair ComponentRegistry needs. This one macro is the entire cost
        // of making a component serializable; the glm members resolve through MathsJson.hpp.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DummyComponent, Position, Size, Color)
    };
}
