#pragma once

#include <cstring>       // std::strcmp — a property is found by its authored name
#include <tuple>
#include <type_traits>

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    inline constexpr LogCategory LogUIBinding{"UIBinding"};

    // =============================================================================
    // UI bindings — a widget PULLS a named value from something the game owns (**UI24**).
    //
    //   MVVM's data half without its notification half: the source is any CReflected object (the
    //   game's view model), read by property NAME through the reflection that already exists; the
    //   widget compares what it read to what it showed and invalidates only on a change, so an
    //   idle canvas stays 0/0. A per-frame pull is what replaces change notification at HUD scale.
    //
    //   A widget names its source as "Source.Property" — one field per bindable property, on the
    //   widget that owns it (UMG's bind slot), never a list on the base.
    // =============================================================================

    /** What a read produced. A widget converts it to what its property needs. */
    struct OPAAX_API UIBoundValue
    {
        enum class EKind : Uint8 { None, Bool, Integer, Number, Text };

        EKind       Kind   = EKind::None;
        double      Number = 0.0;   // Bool / Integer / Number
        OpaaxString Text;           // Text

        /** The value as a string: "true", "3", "0.5" (shortest, %g), or the text itself. */
        OpaaxString ToText() const;
        float       ToFloat() const noexcept { return static_cast<float>(Number); }

        bool operator==(const UIBoundValue& InOther) const noexcept
        {
            return Kind == InOther.Kind && Number == InOther.Number && Text == InOther.Text;
        }
    };

    /** Answers "the value of the property called InProperty", or false when there is no such readable field. */
    using UIBindingReader = TFunction<bool(const char* InProperty, UIBoundValue& OutValue)>;

    /**
     * Read InSource's property named InProperty, by folding its property list.
     *
     * Bool, any integer, any float and OpaaxString are readable; anything else (a Vector, a nested
     * group) answers false — a binding to it is refused loudly by the table, not shown as garbage.
     */
    template<CReflected T>
    bool ReadBoundProperty(const T& InSource, const char* InProperty, UIBoundValue& OutValue)
    {
        bool lFound = false;

        std::apply([&](const auto&... lProperties)
        {
            const auto lTry = [&](const auto& InProp)
            {
                using ValueType = typename std::remove_cvref_t<decltype(InProp)>::ValueType;

                if (lFound || std::strcmp(InProp.Name, InProperty) != 0) { return; }

                const ValueType& lValue = InSource.*(InProp.Member);

                if constexpr (std::is_same_v<ValueType, bool>)
                {
                    OutValue.Kind = UIBoundValue::EKind::Bool;   OutValue.Number = lValue ? 1.0 : 0.0; lFound = true;
                }
                else if constexpr (std::is_integral_v<ValueType>)
                {
                    OutValue.Kind = UIBoundValue::EKind::Integer; OutValue.Number = static_cast<double>(lValue); lFound = true;
                }
                else if constexpr (std::is_floating_point_v<ValueType>)
                {
                    OutValue.Kind = UIBoundValue::EKind::Number;  OutValue.Number = static_cast<double>(lValue); lFound = true;
                }
                else if constexpr (std::is_same_v<ValueType, OpaaxString>)
                {
                    OutValue.Kind = UIBoundValue::EKind::Text;    OutValue.Text = lValue;   lFound = true;
                }
            };

            (lTry(lProperties), ...);
        }, T::GetProperties());

        return lFound;
    }

    /** A reader over InSource — BORROWED: whoever registers it removes it before InSource dies. */
    template<CReflected T>
    UIBindingReader MakeBindingReader(const T& InSource)
    {
        return [&InSource](const char* InProperty, UIBoundValue& OutValue)
        {
            return ReadBoundProperty(InSource, InProperty, OutValue);
        };
    }

    // =============================================================================
    // UIBindingTable — the named sources a canvas's widgets may pull from. Owned by the canvas.
    // =============================================================================
    class OPAAX_API UIBindingTable
    {
    public:
        /** Make InSource readable as InName; a second Add under the same name replaces the first. */
        void Add(OpaaxStringID InName, UIBindingReader InReader);
        void Remove(OpaaxStringID InName);

        bool   Has(OpaaxStringID InName) const noexcept;
        Uint64 Count() const noexcept { return m_Names.size(); }

        /**
         * "Source.Property" → its value. False when the source is not registered or the property
         * is not readable — warned ONCE per path, so a misspelling in a `.opaaxui` is one log
         * line rather than sixty a second.
         */
        bool Read(const OpaaxString& InPath, UIBoundValue& OutValue);

    private:
        TDynArray<OpaaxStringID>   m_Names;
        TDynArray<UIBindingReader> m_Readers;   // parallel to m_Names; a handful of entries, so a scan is right
        TDynArray<OpaaxString>     m_Warned;
    };

    /** InFormat with its first "{}" replaced by InValue — or InValue alone when there is none. */
    OPAAX_API OpaaxString FormatBoundText(const OpaaxString& InFormat, const OpaaxString& InValue);
}
