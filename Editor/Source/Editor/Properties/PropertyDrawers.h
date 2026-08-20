#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // CEnumWithValues — the one drawer that serves every enum
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"     // TResourcePath — the one that serves every resource
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp" // the id the drop target gates on
#include "Editor/Properties/PropertyDrawer.h"
#include "Editor/Resources/ResourceDragDrop.h"

namespace Opaax::Editor
{
    // =============================================================================
    // The BUILT-IN property drawers — the field types the engine's own value vocabulary is made of.
    //
    //   Bodies live in the .cpp so ImGui stays out of every registration site. Declarations only
    //   here, which is all the fold needs.
    //
    //   Dispatch is BY TYPE, which is why LinearColor gets the picker and a bare Vector4F gets four
    //   drags: the type says what the value is, and PropertyMeta says how it behaves (a range).
    //
    //   OpaaxString earned its place when configs started drawing (title, mode, backend, paths). It
    //   is the one built-in with real cost — ImGui's InputText wants a fixed buffer and a copy back,
    //   since imgui_stdlib.cpp is not in the editor's ImGui target — which is exactly why it waited
    //   for a caller instead of being guessed at.
    // =============================================================================

#define OPAAX_DECLARE_PROPERTY_DRAWER(Type)                                                 \
    template<> struct TPropertyDrawer<Type>                                                 \
    { static void Draw(const char* InLabel, Type& InValue, const PropertyMeta& InMeta); }

    OPAAX_DECLARE_PROPERTY_DRAWER(bool);
    OPAAX_DECLARE_PROPERTY_DRAWER(Int16);
    OPAAX_DECLARE_PROPERTY_DRAWER(Int32);
    OPAAX_DECLARE_PROPERTY_DRAWER(Uint32);
    OPAAX_DECLARE_PROPERTY_DRAWER(float);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector2F);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector3F);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector4F);
    OPAAX_DECLARE_PROPERTY_DRAWER(LinearColor);
    OPAAX_DECLARE_PROPERTY_DRAWER(OpaaxString);

#undef OPAAX_DECLARE_PROPERTY_DRAWER

    // =============================================================================
    // EVERY enum at once — one constrained partial specialization, not one per type.
    //
    //   An enum that stamped OPAAX_ENUM_VALUES gets a dropdown from that line alone; the labels are
    //   I11's ToString, so the widget and the log and the file all read the same word. This is the
    //   payoff for making the field a real type: a mode that is not one of the three is no longer
    //   expressible, so nothing downstream needs a fallback for one.
    //
    //   Inline rather than in the .cpp because it is a template — PropertyDrawer.h already carries
    //   <imgui.h> for the same reason.
    // =============================================================================
    // =============================================================================
    // EVERY resource reference at once — the same one-specialization-serves-all shape as the enum
    //   drawer above, and the reason TResourcePath carries its type at all.
    //
    //   The field is filled by DRAGGING a file from the Resource Browser onto it. Typing a path was
    //   the alternative and it is strictly worse authoring: the browser already knows which files
    //   exist and what type each one is, so the only thing a text box adds is the chance to misspell
    //   one. There is no picker button for the same reason a tag picker does not exist yet — the
    //   drawer contract has no EditorContext, deliberately, so the browser is where "what files are
    //   there?" is answered.
    //
    //   A payload of another resource type is REFUSED (no accept highlight, drag stays live), which
    //   is what the type parameter buys: dropping a .wave on a texture field cannot compile a wrong
    //   path into a component.
    // =============================================================================
    template<typename TResource>
    struct TPropertyDrawer<TResourcePath<TResource>>
    {
        static void Draw(const char* InLabel, TResourcePath<TResource>& InValue, const PropertyMeta&)
        {
            ImGui::PushID(InLabel);

            // A button, not a read-only InputText: the button IS the drop target, and it reads as a
            // slot to put something in rather than a field someone forgot to make editable.
            const char* lText = InValue.IsEmpty() ? "(drop a resource here)" : InValue.Path.CStr();
            ImGui::Button(lText, ImVec2(ImGui::CalcItemWidth(), 0.f));

            if (OpaaxString lDropped; AcceptResourceDragPayload(ResourceTypeID::Get<TResource>(), lDropped))
            {
                InValue.Path = Move(lDropped);
            }

            // The full path when it does not fit — the button truncates, and a path that says
            // "Textures/He..." is worse than no path at all when two of them share a prefix.
            if (!InValue.IsEmpty() && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", InValue.Path.CStr());
            }

            if (!InValue.IsEmpty())
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) { InValue.Path = OpaaxString(); }
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(InLabel);

            ImGui::PopID();
        }
    };

    template<CEnumWithValues T>
    struct TPropertyDrawer<T>
    {
        static void Draw(const char* InLabel, T& InValue, const PropertyMeta&)
        {
            if (!ImGui::BeginCombo(InLabel, ToString(InValue)))
            {
                return;
            }

            for (const T lCandidate : TEnumValues<T>::Values)
            {
                const bool bSelected = lCandidate == InValue;

                if (ImGui::Selectable(ToString(lCandidate), bSelected))
                {
                    InValue = lCandidate;
                }

                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
    };
}
