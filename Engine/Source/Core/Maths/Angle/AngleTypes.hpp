#pragma once

#include <Core/Maths/Angle/Angle.h>
#include "Core/Maths/Angle/AngleUnits.inl"   // out-of-line template defs (AsRadian/AsDegree/ToString) — must be visible for local instantiation

namespace Opaax
{
    using FDegree = TDegree<float>;
    using DDegree = TDegree<double>;

    using FRadian = TRadian<float>;
    using DRadian = TRadian<double>;
    
    using FAngle = TAngle<float>;
    using DAngle = TAngle<double>;
}