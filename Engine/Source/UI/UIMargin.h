#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // UIMargin — a quantity per EDGE. Unreal's FMargin, and used for the same two jobs:
    //
    //   `UIImage::Border` — the 9-slice border, in TEXTURE PIXELS.
    //   `UISafeArea::Insets` — how far in from each edge, as a FRACTION of the rect.
    //
    //   The UNIT is the field's, not the type's, which is why there is no range here: a range would
    //   have to be one or the other, and it would only ever reach the editor anyway. **Both
    //   consumers sanitize what they read** — a hand-edited `.opaaxui` is not the inspector, which
    //   is the lesson the inverted anchor left (**UI19**).
    // =============================================================================
    struct UIMargin
    {
        float Left   = 0.f;
        float Right  = 0.f;
        float Bottom = 0.f;
        float Top    = 0.f;

        /** Nothing to do — the caller takes its simple path (**UI20**: no border is no slicing). */
        bool IsZero() const noexcept { return Left == 0.f && Right == 0.f && Bottom == 0.f && Top == 0.f; }

        // _WITH_DEFAULT for UIRect's reason: a missing key must keep the default, never throw.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIMargin, Left, Right, Bottom, Top)

        OPAAX_PROPERTIES(UIMargin,
                         OPAAX_PROP(Left),
                         OPAAX_PROP(Right),
                         OPAAX_PROP(Bottom),
                         OPAAX_PROP(Top))
    };
}
