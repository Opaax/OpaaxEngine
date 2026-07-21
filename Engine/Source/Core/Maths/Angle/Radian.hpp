#pragma once

#include "Core/Maths/TFloatValue.hpp"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    template<CONCEPT_TIsFloat T>
    struct TDegree;

    template<CONCEPT_TIsFloat T>
    struct TRadian final : public TFloatValue<T>   // no OPAAX_API — header-only value template (see TAngle)
    {
        using Base = TFloatValue<T>;

        using Base::Base;
        using Base::m_value;
        
        // =============================================================================
        // Ctor
        // =============================================================================

        constexpr TRadian() = default;
        
        // =============================================================================
        // Function
        // =============================================================================

        [[nodiscard]] constexpr TDegree<T> AsDegree() const;
        [[nodiscard]] OpaaxString ToString() const { return ToString(2); }
        [[nodiscard]] OpaaxString ToString(Uint8 Precision) const;
        
        // =============================================================================
        // Operators
        // =============================================================================


        constexpr TRadian operator+(const TRadian& Other) const
        {
            return TRadian(m_value + Other.m_value);
        }

        constexpr TRadian operator-(const TRadian& Other) const
        {
            return TRadian(m_value - Other.m_value);
        }

        constexpr TRadian operator*(const TRadian& Other) const
        {
            return TRadian(m_value * Other.m_value);
        }

        constexpr TRadian operator/(const TRadian& Other) const
        {
            return TRadian(m_value / Other.m_value);
        }

        constexpr TRadian& operator+=(const TRadian& Other)
        {
            m_value += Other.m_value;
            return *this;
        }

        constexpr TRadian& operator-=(const TRadian& Other)
        {
            m_value -= Other.m_value;
            return *this;
        }

        constexpr TRadian& operator*=(const TRadian& Other)
        {
            m_value *= Other.m_value;
            return *this;
        }

        constexpr TRadian& operator/=(const TRadian& Other)
        {
            m_value /= Other.m_value;
            return *this;
        }
    };
}
