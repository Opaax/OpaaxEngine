#pragma once
#include <limits>

/**
 * for checking super long value.
 * std::cout << std::setprecision (std::numeric_limits<double>::digits - 1)
 */

namespace OPAAX
{
    namespace MATH
    {
        /*---------------------------------------------------------
         * FLOATS
         *--------------------------------------------------------*/

        /*------------------------ PI ----------------------------*/
        constexpr float OP_PI = 3.1415926535897932f;
        constexpr float OP_LONG_PI = 3.1415926535897932384626433832795f;
        constexpr float OP_INV_PI = 0.31830988618f; // 1/PI
        constexpr float OP_HALF_PI = 1.57079632679f; // PI/2
        constexpr float OP_LONG_HALF_PI = 1.5707963267948966192313216916398f;
        constexpr float OP_TWO_PI = 6.28318530717f; // 2 * PI
        constexpr float OP_PI_SQUARED = 9.86960440108f; // PI * PI
        constexpr float OP_QUARTER_PI = 0.78539816339f; // PI/4
        constexpr float OP_THIRD_PI = 1.04719758033f; // PI/3
        constexpr float OP_HALF_CIRCLE = 180.f;
        constexpr float OP_CIRCLE = 360.f;

        /*------------------------ NUM ----------------------------*/
        constexpr float OP_ZERO = 0.0f;
        constexpr float OP_SMALL_NUMBER = 1.e-8f;
        constexpr float OP_HALF_SMALL_NUMBER = 1.e-4f;
        constexpr float OP_FLOAT_MAX = 3.402823466e+38F; //@see FLT_MAX
        constexpr float OP_FLOAT_MIN = 1.175494351e-38F; //@see FLT_MIN
        /***
         * smallest such that 1.0+OP_EPSILON != 1.0;
         *
         * @see FLT_EPSILON float.h
         */
        constexpr float OP_EPSILON = 1.192092896e-07f;

        /**
         * Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2
         */
        constexpr float OP_GOLDEN_RATIO = 1.6180339887498948482045868343656381f;
        constexpr float OP_HUNDRED_PERCENT = 100.f;

        /*------------------------ BOUNDS ----------------------------*/
        /**
         * Copied from float.h
         */
        constexpr float OP_MAX_FLOAT = 3.402823466e+38f;

        /*------------------------ SQRT ----------------------------*/
        constexpr float OP_SQRT_2 = 1.4142135623730950488016887242097f;
        constexpr float OP_SQRT_3 = 1.7320508075688772935274463415059f;
        constexpr float OP_INV_SQRT_2 = 0.70710678118654752440084436210485f;
        constexpr float OP_INV_SQRT_3 = 0.57735026918962576450914878050196f;
        constexpr float OP_HALF_SQRT_2 = 0.70710678118654752440084436210485f;
        constexpr float OP_HALF_SQRT_3 = 0.86602540378443864676372317075294f;

        /*------------------------ Conversion ----------------------------*/
        /**
         * Unit to convert values
         * 1 km to meter = 1 * OP_KM_TO_M
         */
        constexpr float OP_KM_TO_M = 1000.f;
        constexpr float OP_M_TO_KM = 0.001f;
        constexpr float OP_CM_TO_M = 0.01f;
        constexpr float OP_M_TO_CM = 100.f;
        constexpr float OP_CM2_TO_M2 = 0.0001f;
        constexpr float OP_M2_TO_CM2 = 10000.f;
        constexpr float OP_RAD_TO_DEG = OP_HALF_CIRCLE / OP_PI;
        constexpr float OP_DEG_TO_RAD = OP_PI / OP_HALF_CIRCLE;

        /*---------------------------------------------------------
         * DOUBLE
         *--------------------------------------------------------*/
        /*------------------------ PI ----------------------------*/
        constexpr double OP_DOUBLE_PI = 3.14159265358979323846264;
        constexpr double OP_DOUBLE_LONG_PI = 3.141592653589793238462643383279502884197169399;
        constexpr double OP_DOUBLE_INV_PI = 0.31830988618; // 1/PI
        constexpr double OP_DOUBLE_HALF_PI = 1.57079632679; // PI/2
        constexpr double OP_DOUBLE_LONG_HALF_PI = 1.570796326794896557998981734272092580795288085;
        constexpr double OP_DOUBLE_TWO_PI = 6.28318530717; // 2 * PI
        constexpr double OP_DOUBLE_PI_SQUARED = 9.86960440108; // PI * PI
        constexpr double OP_DOUBLE_QUARTER_PI = 0.785398185253143310546875; // PI/4
        constexpr double OP_DOUBLE_THIRD_PI = 1.0471975803375244140625; // PI/3
        constexpr double OP_DOUBLE_HALF_CIRCLE = 180.;
        constexpr double OP_DOUBLE_CIRCLE = 360.;

        /*------------------------ NUM ----------------------------*/
        constexpr double OP_DOUBLE_ZERO = 0.0;
        constexpr double OP_DOUBLE_SMALL_NUMBER = 1.e-8;
        constexpr double OP_DOUBLE_HALF_SMALL_NUMBER = 1.e-4;
        constexpr double OP_DOUBLE_BIG_NUMBER = 3.4e+38;
        /**
         * smallest such that 1.0+OP_DOUBLE_EPSILON != 1.0
         *
         * @see DBL_EPSILON float.h
         */
        constexpr double OP_DOUBLE_EPSILON = 2.2204460492503131e-016;
        /**
         * Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2
         */
        constexpr double OP_DOUBLE_GOLDEN_RATIO = 1.6180339887498948482045868343656381;
        constexpr double OP_DOUBLE_HUNDRED_PERCENT = 100.;

        /*------------------------ SQRT ----------------------------*/
        constexpr double OP_DOUBLE_SQRT_2 = 1.4142135623730950488016887242097;
        constexpr double OP_DOUBLE_SQRT_3 = 1.7320508075688772935274463415059;
        constexpr double OP_DOUBLE_INV_SQRT_2 = 0.70710678118654752440084436210485;
        constexpr double OP_DOUBLE_INV_SQRT_3 = 0.57735026918962576450914878050196;
        constexpr double OP_DOUBLE_HALF_SQRT_2 = 0.70710678118654752440084436210485;
        constexpr double OP_DOUBLE_HALF_SQRT_3 = 0.86602540378443864676372317075294;

        /*------------------------ Conversion ----------------------------*/
        /**
         * Unit to convert values
         * 1 km to meter = 1 * OP_KM_TO_M
         */
        constexpr double OP_DOUBLE_KM_TO_M = 1000.;
        constexpr double OP_DOUBLE_M_TO_KM = 0.001;
        constexpr double OP_DOUBLE_CM_TO_M = 0.01;
        constexpr double OP_DOUBLE_M_TO_CM = 100;
        constexpr double OP_DOUBLE_CM2_TO_M2 = 0.0001;
        constexpr double OP_DOUBLE_M2_TO_CM2 = 10000.;
        constexpr double OP_DOUBLE_RAD_TO_DEG = OP_DOUBLE_HALF_CIRCLE / OP_DOUBLE_PI;
        constexpr double OP_DOUBLE_DEG_TO_RAD = OP_DOUBLE_PI / OP_DOUBLE_HALF_CIRCLE;
    }
}
