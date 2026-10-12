#pragma once
#include "Core/Maths/Angle/Radian.hpp"
#include "Core/Maths/Angle/Degree.hpp"
#include "Core/Maths/Angle/AngleTypes.hpp"
#include "Core/Maths/Maths.h"


namespace Opaax
{
    template<CFloatingPoint T>
    constexpr TRadian<T> TDegree<T>::AsRadian() const
    {
        return TRadian<T>(static_cast<T>(Maths::DegreesToRadians(m_value)));
    }

    template<CFloatingPoint T>
    constexpr TDegree<T> TRadian<T>::AsDegree() const
    {
        return TDegree<T>( static_cast<T>( Maths::RadiansToDegrees(m_value) ) );
    }
    
    template<CFloatingPoint T>
    OpaaxString TDegree<T>::ToString(Uint8 Precision) const
    {
        return OpaaxString(fmt::format("{:.{}} Degree", m_value, static_cast<int>(Precision)).c_str());
    }
    
    template<CFloatingPoint T>
    OpaaxString TRadian<T>::ToString(Uint8 Precision) const
    {
        return OpaaxString(fmt::format("{:.{}} Radian", m_value, static_cast<int>(Precision)).c_str());
    }
}
