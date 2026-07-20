#pragma once

#include "Core/EngineAPI.h"

#include <Core/Maths/Angle/Degree.hpp>
#include <Core/Maths/Angle/Radian.hpp>
#include <glm/detail/qualifier.hpp>

namespace Opaax
{
    /**
     * @class TAngle
     * Represent an angle in the engine.
     * 
     * @tparam T The angle type float or double
     */
    // Header-only value template — NO OPAAX_API. dllexport/dllimport on a class TEMPLATE exports nothing
    // (a template is not code until instantiated); marking it dllimport makes consumers expect the
    // instantiation from the DLL, which never exports it -> LNK2019. Stateless value types are DLL-safe by
    // construction and instantiate per-TU (same convention as TFloatValue<T>).
    template<CONCEPT_TIsFloat T>
    class TAngle
    {
    public:

        using ValueType = T;
        using DegreeType = TDegree<ValueType>;
        using RadianType = TRadian<ValueType>;
        
        // =============================================================================
        // Ctors
        // =============================================================================

        constexpr TAngle() = default;

        constexpr explicit TAngle(DegreeType InDegree)
            : m_Radian(InDegree.AsRadian())
        {
        }

        constexpr explicit TAngle(RadianType InRadian)
            : m_Radian(InRadian)
        {
        }
        
        // =============================================================================
        // Function
        // =============================================================================
    public:
        [[nodiscard]] constexpr DegreeType AsDegree() const
        {
            return m_Radian.AsDegree();
        }

        [[nodiscard]] constexpr RadianType AsRadian() const
        {
            return m_Radian;
        }
        
        OpaaxString ToString()
        {
            return ToString(5);
        }
        
        OpaaxString ToString(Uint8 Precision)
        {
            OpaaxString lRad = m_Radian.ToString(Precision);
            OpaaxString lDeg = m_Radian.AsDegree().ToString(Precision);
            
            return OpaaxString("Angle: ") + lRad + "----" + lDeg;
        }
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        RadianType m_Radian;
    };
}
