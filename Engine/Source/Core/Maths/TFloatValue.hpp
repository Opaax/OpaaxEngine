#pragma once

#include "Core/OpaaxTRequire.hpp"
#include <compare>

namespace Opaax
{
    template <CONCEPT_TIsFloat T>
    struct TFloatValue
    {
        using ValueType = T;
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================

        constexpr TFloatValue() = default;

        constexpr explicit TFloatValue(T InValue)
            : m_value(InValue) {}
        
        // =============================================================================
        // Function
        // =============================================================================

        // =============================================================================
        // Get - Set
        
        /***/
        [[nodiscard]] constexpr ValueType GetValue() const { return m_value; }
        /***/
        constexpr void SetValue(ValueType InValue) { m_value = InValue; }
        
        // End Get - Set
        // =============================================================================
        
        // =============================================================================
        // Operators
        // =============================================================================
        auto operator<=>(const TFloatValue&) const = default;

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        ValueType m_value{};
    };
}
