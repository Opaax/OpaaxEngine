#pragma once
#include "Opaax/OpaaxTRequire.h"

/**
 * Non-macro declaration of the most commonly used types to assist Intellisense.
 * @see Unreal MathFwd.h
 */
namespace OPAAX::MATH
{
    // Forward declaration of templates
    template<CONCEPT_TIsFloat T> struct OpaaxTVector3D;
    template<CONCEPT_TIsFloat T> struct OpaaxTVector2D;
}

using FVector2D = OPAAX::MATH::OpaaxTVector2D<float>;
using DVector2D = OPAAX::MATH::OpaaxTVector2D<double>;

using FVector3D = OPAAX::MATH::OpaaxTVector3D<float>;
using DVector3D = OPAAX::MATH::OpaaxTVector3D<double>;