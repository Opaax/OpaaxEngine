#pragma once

#include <cstring>       // std::strcmp
#include <tuple>
#include <type_traits>

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    inline constexpr LogCategory LogUIBinding{"UIBinding"};

    // =============================================================================
    // UI bindings — a widget reads a named value from a game object every frame.
    //   The source is any reflected object, read by property name. A widget invalidates only
    //   when the value changed, so an idle canvas does no work.
    //   A widget names its source as "Source.Property", one field per bindable property.
    // =============================================================================

    /** A read value. The widget converts it to what it needs. */
    struct UIBoundValue
    {
        enum class EKind : Uint8 { None, Bool, Integer, Number, Text };

        EKind       Kind   = EKind::None;
        double      Number = 0.0;   // Bool / Integer / Number
        OpaaxString Text;           // Text

        /** The value as text: "true", "3", "0.5" (%g), or the text itself. */
        OpaaxString ToText() const;
        float       ToFloat() const noexcept { return static_cast<float>(Number); }

        bool operator==(const UIBoundValue& InOther) const noexcept
        {
            return Kind == InOther.Kind && Number == InOther.Number && Text == InOther.Text;
        }
    };

    /** Reads the property named InProperty; false if there is no readable field with that name. */
    using UIBindingReader = TFunction<bool(const char* InProperty, UIBoundValue& OutValue)>;

    /**
     * Reads InSource's property InProperty through its property list.
     * Bool, integers, floats and OpaaxString are readable; anything else returns false.
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

    /** A reader over InSource (borrowed: remove it before InSource is destroyed). */
    template<CReflected T>
    UIBindingReader MakeBindingReader(const T& InSource)
    {
        return [&InSource](const char* InProperty, UIBoundValue& OutValue)
        {
            return ReadBoundProperty(InSource, InProperty, OutValue);
        };
    }

    /**
     * Returned by Add, taken by Remove, so an owner only removes its own source (during a level
     * change both worlds register "Hud"; the old one's Remove must not delete the new one).
     */
    struct UIBindingHandle
    {
        OpaaxStringID Name;
        Uint64        Ticket = 0;   // 0 = never registered

        bool IsValid() const noexcept { return Ticket != 0; }
    };

    // =============================================================================
    // UIBindingTable — the named sources a canvas's widgets can read. Owned by the canvas.
    // =============================================================================
    class UIBindingTable
    {
    public:
        /** Registers InSource as InName; a second Add with the same name replaces the first. */
        UIBindingHandle Add(OpaaxStringID InName, UIBindingReader InReader);

        /** Removes the source InHandle registered (nothing if the name was re-added since). */
        void Remove(const UIBindingHandle& InHandle);

        bool   Has(OpaaxStringID InName) const noexcept;
        Uint64 Count() const noexcept { return m_Names.size(); }

        /**
         * "Source.Property" -> its value. False if the source or property is unknown (warned once per path).
         */
        bool Read(const OpaaxString& InPath, UIBoundValue& OutValue);

    private:
        TDynArray<OpaaxStringID>   m_Names;
        TDynArray<UIBindingReader> m_Readers;   // same order as m_Names
        TDynArray<Uint64>          m_Tickets;   // same order: which Add owns the entry
        TDynArray<OpaaxString>     m_Warned;
        Uint64                     m_NextTicket = 1;
    };

    /** InFormat with its first "{}" replaced by InValue, or InValue alone when there is none. */
    OpaaxString FormatBoundText(const OpaaxString& InFormat, const OpaaxString& InValue);
}
