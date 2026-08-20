#pragma once

#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // LinearColor — RGBA as four linear floats.
    //
    //   A DISTINCT TYPE, not an alias, and that is the whole point: it is what lets the editor pick a
    //   colour picker without being told. Dispatch is TPropertyDrawer<LinearColor> versus
    //   TPropertyDrawer<Vector4F>, and an alias cannot carry an overload — which is why the property
    //   system briefly had a "this Vector4F is a colour" hint instead. A hint that restates what a
    //   type could say is a weaker version of the type.
    //
    //   It IS a Vector4F rather than holding one, so `.r/.g/.b/.a` and `.x/.y/.z/.w` both work, every
    //   renderer call site that speaks vectors keeps compiling (the conversion is the base binding,
    //   not a user conversion), and glm's own operators still apply. Slicing a LinearColor to a
    //   Vector4F is exactly the intended conversion, and glm's vectors are plain data — no vtable,
    //   nothing to lose.
    //
    //   Its json is the VECTOR's (LinearColorJson.h): the type is about the editor and the call
    //   sites, never about the file, so nothing already saved moves.
    //
    //   Named LinearColor rather than Color because a member named `Color` of type `Color` shadows
    //   its own type inside the struct holding it — the trap EditorContext::Route documents. This
    //   way DummyComponent::Color keeps its name, and so does its key on disk.
    //
    //   Header-only value type: no OPAAX_API (I6), no state of its own.
    // =============================================================================
    struct LinearColor : Vector4F
    {
        LinearColor() : Vector4F(1.f, 1.f, 1.f, 1.f) {}

        LinearColor(const Vector4F& InValue) : Vector4F(InValue) {}
        LinearColor(const float InR, const float InG, const float InB, const float InA = 1.f)
            : Vector4F(InR, InG, InB, InA)
        {
        }
    };
}
