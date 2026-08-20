#pragma once

#include "Core/Maths/MathTypes.h"
#include "Editor/Properties/PropertyDrawer.h"

namespace Opaax::Editor
{
    // =============================================================================
    // The BUILT-IN property drawers — the field types the engine's own value vocabulary is made of.
    //
    //   Bodies live in the .cpp so ImGui stays out of every registration site. Declarations only
    //   here, which is all the fold needs.
    //
    //   OpaaxString is deliberately ABSENT: it is the one built-in with real design cost (ImGui's
    //   InputText wants a fixed buffer and a copy back, since imgui_stdlib.cpp is not in the editor's
    //   ImGui target) and no component in the tree has a string field. It waits for its first caller;
    //   until then a string property is a compile error, which is the intended failure.
    // =============================================================================

#define OPAAX_DECLARE_PROPERTY_DRAWER(Type)                                        \
    template<> struct TPropertyDrawer<Type>                                        \
    { static void Draw(const char* InLabel, Type& InValue, EPropertyHint InHint); }

    OPAAX_DECLARE_PROPERTY_DRAWER(bool);
    OPAAX_DECLARE_PROPERTY_DRAWER(Int32);
    OPAAX_DECLARE_PROPERTY_DRAWER(Uint32);
    OPAAX_DECLARE_PROPERTY_DRAWER(float);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector2F);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector3F);

    // The one type whose widget the hint decides: RGBA colour, or four numbers.
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector4F);

#undef OPAAX_DECLARE_PROPERTY_DRAWER
}
