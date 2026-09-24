
#include <doctest.h>

#include "Core/Maths/Angle/AngleTypes.hpp" 

using namespace Opaax;

TEST_CASE("Converting Degree to radian and vice versa should be same value")
{
	 FDegree lDeg = FDegree(90);
     FRadian lRad = FRadian(lDeg.AsRadian());
        
    CHECK(lDeg.AsRadian() == lRad);
    CHECK(lDeg == lRad.AsDegree());
}