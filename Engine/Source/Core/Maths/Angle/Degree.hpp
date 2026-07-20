#pragma once

#include "Core/Maths/TFloatValue.hpp"
#include "Core/OpaaxString.hpp"
#include "Core/EngineAPI.h"

namespace Opaax
{
    template<CONCEPT_TIsFloat T>
    struct TRadian;

    template<CONCEPT_TIsFloat T>
    struct TDegree final : public TFloatValue<T>   // no OPAAX_API — header-only value template (see TAngle)
    {
        using Base = TFloatValue<T>;

        using Base::Base;
        using Base::m_value;
        
        // =============================================================================
        // Ctor
        // =============================================================================

        constexpr TDegree() = default;
        
        // =============================================================================
        // Function
        // =============================================================================

        [[nodiscard]] constexpr TRadian<T> AsRadian() const;
        [[nodiscard]] OpaaxString ToString() const { return ToString(2); }
        [[nodiscard]] OpaaxString ToString(Uint8 Precision) const;

        // =============================================================================
        // Operators
        // =============================================================================
        

        constexpr TDegree operator+(const TDegree& Other) const
        {
            return TDegree(m_value + Other.m_value);
        }

        constexpr TDegree operator-(const TDegree& Other) const
        {
            return TDegree(m_value - Other.m_value);
        }

        constexpr TDegree operator*(const TDegree& Other) const
        {
            return TDegree(m_value * Other.m_value);
        }

        constexpr TDegree operator/(const TDegree& Other) const
        {
            return TDegree(m_value / Other.m_value);
        }

        constexpr TDegree& operator+=(const TDegree& Other)
        {
            m_value += Other.m_value;
            return *this;
        }

        constexpr TDegree& operator-=(const TDegree& Other)
        {
            m_value -= Other.m_value;
            return *this;
        }

        constexpr TDegree& operator*=(const TDegree& Other)
        {
            m_value *= Other.m_value;
            return *this;
        }

        constexpr TDegree& operator/=(const TDegree& Other)
        {
            m_value /= Other.m_value;
            return *this;
        }
    };
}
