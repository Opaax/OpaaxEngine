#pragma once

#include <tuple>

#include "Core/EngineAPI.h"   // BIT — the flags below
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // A PROPERTY LIST — the fields a type is willing to have edited, as DATA.
    //
    //   ANY type, not just a component: this header sits in Core and knows nothing about the World.
    //   A component was the first caller (so a component that is two floats does not need a
    //   hand-written drawer); a config data type is the obvious second, since IConfig already
    //   type-erases at TConfig<TData> the way DrawerRegistry does at Register<TComponent>().
    //   Whoever folds over the list picks a widget per field type — the list itself never knows.
    //
    //   Nothing here is editor-only: the engine builds no ImGui (D4), and a component header is
    //   compiled into the engine DLL (OPAAX_WITH_EDITOR=0) *and* into the editor exe (=1), so an
    //   `#if` around any of this would be one type with two definitions in one program.
    //
    //   It costs a shipped game nothing: a static constexpr table nobody references is never emitted.
    //
    //   Header-only value templates — no OPAAX_API (I6), no registry, no static (I1). A field type
    //   the editor cannot draw is a COMPILE error at the registration line, never a blank row.
    // =============================================================================

    /**
     * How a value BEHAVES — the part a type genuinely cannot state.
     *
     * Scoped, so enumerators do not leak, but bitmaskable through the operator below — the shape
     * EEventCategory settled on ([[L4]]).
     */
    enum class EPropertyFlags : Uint8
    {
        None        = 0,
        /** Editing it does nothing until the next launch, because whoever reads it reads it at boot. */
        NeedRestart = BIT(0),
    };

    constexpr EPropertyFlags operator|(const EPropertyFlags InA, const EPropertyFlags InB) noexcept
    {
        return static_cast<EPropertyFlags>(static_cast<Uint8>(InA) | static_cast<Uint8>(InB));
    }

    constexpr bool HasFlag(const EPropertyFlags InValue, const EPropertyFlags InFlag) noexcept
    {
        return (static_cast<Uint8>(InValue) & static_cast<Uint8>(InFlag)) != 0;
    }

    /**
     * What a field's TYPE cannot say about it.
     *
     * Deliberately NOT "which widget to use" — that is the type's job, and a hint that restates it is
     * a weaker version of a type (which is why LinearColor exists instead of a Color hint). What
     * belongs here is behaviour: the bounds a value must stay inside, and — from S2 — whether
     * changing it takes effect now or at the next launch.
     *
     * An unset range is Min == Max, which every ImGui drag reads as "unbounded", so the ordinary
     * property needs no branch and no extra flag.
     */
    struct PropertyMeta
    {
        float          RangeMin = 0.f;
        float          RangeMax = 0.f;
        EPropertyFlags Flags    = EPropertyFlags::None;
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
        PropertyMeta     Meta;

        /**
         * Clamp the value between InMin and InMax.
         *
         * Chained facet, returning a modified COPY — the list is constexpr, so nothing is mutated.
         * Same shape EditorMenuCommandNode's SetEnabled/SetChecked use at the call site.
         */
        constexpr TProperty SetRange(const float InMin, const float InMax) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.RangeMin = InMin;
            lCopy.Meta.RangeMax = InMax;

            return lCopy;
        }

        /**
         * State how the value behaves. Settable on a GROUP (a property whose type is itself
         * CReflected), which is what keeps "this whole block needs a restart" one statement rather
         * than one per field.
         */
        constexpr TProperty SetFlags(const EPropertyFlags InFlags) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.Flags = InFlags;

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
//       OPAAX_PROP(Size).SetRange(1.f, 4096.f))
// =============================================================================
#define OPAAX_PROPERTIES(ClassName, ...)                                            \
    using PropertyOwnerType = ClassName;                                            \
    static constexpr auto GetProperties() noexcept                                  \
    { return ::std::make_tuple(__VA_ARGS__); }

#define OPAAX_PROP(Field) ::Opaax::MakeProperty(#Field, &PropertyOwnerType::Field)
