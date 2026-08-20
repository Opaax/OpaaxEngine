#pragma once

#include <tuple>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // A component's PROPERTY LIST — the fields it is willing to have edited, as DATA.
    //
    //   It exists so a component that is two floats does not need a hand-written drawer: the editor
    //   folds over this list and picks a widget per field type. Nothing here knows about ImGui, and
    //   nothing here is editor-only — the engine builds no ImGui (D4), and a component header is
    //   compiled into the engine DLL (OPAAX_WITH_EDITOR=0) *and* into the editor exe (=1), so an
    //   `#if` around any of this would be one type with two definitions in one program.
    //
    //   It costs a shipped game nothing: a static constexpr table nobody references is never emitted.
    //
    //   Header-only value templates — no OPAAX_API (I6), no registry, no static (I1). A field type
    //   the editor cannot draw is a COMPILE error at the registration line, never a blank row.
    // =============================================================================

    /**
     * How a field wants to be edited, when its C++ type does not say.
     *
     * A Vector4F is four numbers or an RGBA colour, and only the author knows which. Kept to the
     * cases that have a real caller — this is a growth point, not a taxonomy.
     */
    enum class EPropertyHint : Uint8
    {
        None,
        Color
    };

    // =============================================================================
    // TProperty — one field: its authoring name and how to reach it.
    //
    //   The member pointer carries BOTH types, so a field states its name once and never its type.
    //   That is what keeps new field types out of the engine: supporting one is an editor-side
    //   specialization, never a new FLOAT_PROP/INT_PROP macro here.
    // =============================================================================
    template<typename TClass, typename TValue>
    struct TProperty
    {
        using ClassType = TClass;
        using ValueType = TValue;

        const char*      Name   = nullptr;
        TValue TClass::* Member = nullptr;
        EPropertyHint    Hint   = EPropertyHint::None;

        /**
         * Chained facet, returning a modified COPY — the list is constexpr, so nothing is mutated.
         * Same shape EditorMenuCommandNode's SetEnabled/SetChecked use at the call site.
         */
        constexpr TProperty SetHint(const EPropertyHint InHint) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Hint = InHint;

            return lCopy;
        }
    };

    template<typename TClass, typename TValue>
    constexpr TProperty<TClass, TValue> MakeProperty(const char* InName, TValue TClass::* InMember) noexcept
    {
        return TProperty<TClass, TValue>{InName, InMember};
    }

    /**
     * A type that describes its fields. Detected, optional, no base class — the CComponent /
     * CResource shape (I8). A component without it is simply not drawn generically.
     */
    template<typename T>
    concept CReflected = requires { T::GetProperties(); };
}

// =============================================================================
// Stamp on a type whose fields the editor should be able to draw. Sits beside
// NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT, in the same public section:
//
//   OPAAX_PROPERTIES(DummyComponent,
//       OPAAX_PROP(Position),
//       OPAAX_PROP(Color).SetHint(EPropertyHint::Color))
// =============================================================================
#define OPAAX_PROPERTIES(ClassName, ...)                                            \
    using PropertyOwnerType = ClassName;                                            \
    static constexpr auto GetProperties() noexcept                                  \
    { return ::std::make_tuple(__VA_ARGS__); }

#define OPAAX_PROP(Field) ::Opaax::MakeProperty(#Field, &PropertyOwnerType::Field)
