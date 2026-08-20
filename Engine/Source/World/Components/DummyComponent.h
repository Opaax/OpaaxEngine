#pragma once

#include <nlohmann/json.hpp>

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
        Vector2F Position = { 0.f, 0.f };
        Vector2F Size     = { 50.f, 50.f };
        Vector4F Color    = { 1.f, 1.f, 1.f, 1.f };
        float Test = 1;
        float Test2 = 2;
        float Test3 = 3;

        // Satisfies CComponent (World/Components/ComponentConcept.hpp) — generates the
        // to_json/from_json pair ComponentRegistry needs. This one macro is the entire cost
        // of making a component serializable; the glm members resolve through MathsJson.hpp.
        //
        // _WITH_DEFAULT is the required variant, not a preference: the plain macro reads every
        // field with at(), which THROWS on a missing key, so adding a field here would refuse
        // to open every map saved before it existed — at boot, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DummyComponent, Position, Size, Color, Test, Test2, Test3)

        // What the Inspector draws, with no drawer written for it. The hint is here because a
        // Vector4F is four numbers or a colour and only the author knows which.
        OPAAX_PROPERTIES(DummyComponent,
                         OPAAX_PROP(Position),
                         OPAAX_PROP(Size),
                         OPAAX_PROP(Color).SetHint(EPropertyHint::Color),
                         OPAAX_PROP(Test),
                         OPAAX_PROP(Test2),
                         OPAAX_PROP(Test3))
    };
}
