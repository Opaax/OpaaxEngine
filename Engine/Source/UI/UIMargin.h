#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // UIMargin — a value per edge (like Unreal's FMargin). The unit depends on the field:
    //   UIImage::Border is in texture pixels, UISafeArea::Insets in fractions of the rect.
    //   Users sanitize the values.
    // =============================================================================
    struct UIMargin
    {
        float Left   = 0.f;
        float Right  = 0.f;
        float Bottom = 0.f;
        float Top    = 0.f;

        /** All zero (no border, no inset). */
        bool IsZero() const noexcept { return Left == 0.f && Right == 0.f && Bottom == 0.f && Top == 0.f; }

        // _WITH_DEFAULT: a missing key keeps its default.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIMargin, Left, Right, Bottom, Top)

        OPAAX_PROPERTIES(UIMargin,
                         OPAAX_PROP(Left),
                         OPAAX_PROP(Right),
                         OPAAX_PROP(Bottom),
                         OPAAX_PROP(Top))
    };
}
