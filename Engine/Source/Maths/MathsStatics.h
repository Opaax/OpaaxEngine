#pragma once

#include <limits>

/**
 * for checking super long value.
 * std::cout << std::setprecision (std::numeric_limits<double>::digits - 1)
 */

namespace Opaax
{
    // =============================================================================
    // Float

    // =============================================================================
    // PI
    constexpr float FPI                 = 3.1415926535897932f;
    constexpr float FLONG_PI            = 3.1415926535897932384626433832795f;
    constexpr float FINV_PI             = 0.31830988618f; // 1/PI
    constexpr float FHALF_PI            = 1.57079632679f; // PI/2
    constexpr float FLONG_HALF_PI       = 1.5707963267948966192313216916398f;
    constexpr float FTWO_PI             = 6.28318530717f; // 2 * PI
    constexpr float FPI_SQUARED         = 9.86960440108f; // PI * PI
    constexpr float FQUARTER_PI         = 0.78539816339f; // PI/4
    constexpr float FTHIRD_PI           = 1.04719758033f; // PI/3
    constexpr float FHALF_CIRCLE_DEGREE = 180.f;
    constexpr float FCIRCLE_DEGREE      = 360.f;
    // End PI
    // =============================================================================
    
    // =============================================================================
    // Num
    constexpr float FZERO               = 0.0f;
    constexpr float FONE                = 1.0f;
    constexpr float FONE_HALF           = 0.5f;
    constexpr float FSMALL_NUMBER       = 1.e-8f;
    constexpr float FHALF_SMALL_NUMBER  = 1.e-4f;
    constexpr float FLOAT_MAX           = 3.402823466e+38F; //@see FLT_MAX
    constexpr float FLOAT_MIN           = 1.175494351e-38F; //@see FLT_MIN
    
    /***
     * smallest such that 1.0+OP_EPSILON != 1.0;
     * @see FLT_EPSILON float.h
     */
    constexpr float FEPSILON = 1.192092896e-07f;
    
    /**
     * Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2
     */
    constexpr float FGOLDEN_RATIO       = 1.6180339887498948482045868343656381f;
    constexpr float FHUNDRED_PERCENT    = 100.f;
    // End Num
    // =============================================================================
    
    // =============================================================================
    // SQRT
    constexpr float FSQRT_2         = 1.4142135623730950488016887242097f;
    constexpr float FSQRT_3         = 1.7320508075688772935274463415059f;
    constexpr float FINV_SQRT_2     = 0.70710678118654752440084436210485f;
    constexpr float FINV_SQRT_3     = 0.57735026918962576450914878050196f;
    constexpr float FHALF_SQRT_2    = 0.70710678118654752440084436210485f;
    constexpr float FHALF_SQRT_3    = 0.86602540378443864676372317075294f;
    // End SQRT
    // =============================================================================
    
    // End Float
    // =============================================================================
    
    // =============================================================================
    // Double
    // =============================================================================
    // PI
    constexpr double DPI                    = 3.14159265358979323846264;
    constexpr double DLONG_PI               = 3.141592653589793238462643383279502884197169399;
    constexpr double DINV_PI                = 0.31830988618; // 1/PI
    constexpr double DHALF_PI               = 1.57079632679; // PI/2
    constexpr double DLONG_HALF_PI          = 1.570796326794896557998981734272092580795288085;
    constexpr double DTWO_PI                = 6.28318530717; // 2 * PI
    constexpr double DPI_SQUARED            = 9.86960440108; // PI * PI
    constexpr double DQUARTER_PI            = 0.785398185253143310546875; // PI/4
    constexpr double DTHIRD_PI              = 1.0471975803375244140625; // PI/3
    constexpr double DHALF_CIRCLE_DEGREE    = 180.;
    constexpr double DCIRCLE_DEGREE         = 360.;
    // End PI
    // =============================================================================
    
    // =============================================================================
    // Num
    constexpr double DZERO                      = 0.0;
    constexpr double DONE                       = 1.0;
    constexpr double DONE_HALF                  = 0.5;
    constexpr double DSMALL_NUMBER              = 1.e-8;
    constexpr double DOUBLE_HALF_SMALL_NUMBER   = 1.e-4;
    constexpr double DOUBLE_MAX                 = 1.7976931348623158e+308; //@see DBL_MAX
    constexpr double DOUBLE_MIN                 = 2.2250738585072014e-308; //@see DBL_MIN
    /**
     * smallest such that 1.0+OP_DOUBLE_EPSILON != 1.0
     *
     * @see DBL_EPSILON float.h
     */
    constexpr double DEPSILON = 2.2204460492503131e-016;
    /**
     * Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2
     */
    constexpr double DGOLDEN_RATIO      = 1.6180339887498948482045868343656381;
    constexpr double DHUNDRED_PERCENT   = 100.;
    // End Num
    // =============================================================================
    
    // =============================================================================
    // SQRT
    constexpr float DSQRT_2         = 1.4142135623730950488016887242097;
    constexpr float DSQRT_3         = 1.7320508075688772935274463415059;
    constexpr float DINV_SQRT_2     = 0.70710678118654752440084436210485;
    constexpr float DINV_SQRT_3     = 0.57735026918962576450914878050196;
    constexpr float DHALF_SQRT_2    = 0.70710678118654752440084436210485;
    constexpr float DHALF_SQRT_3    = 0.86602540378443864676372317075294;
    // End SQRT
    // =============================================================================
    
    // End Double
    // =============================================================================
}
