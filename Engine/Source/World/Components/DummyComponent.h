#pragma once

#include "Core/Maths/MathTypes.h"
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
    };
}
