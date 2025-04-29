#pragma once
#include "OpaaxMathConst.h"
#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/OpaaxTRequire.h"

namespace OPAAX
{
    namespace MATH
    {
        class OPAAX_API OpaaxMath
        {
        public:
            //-----------------------------------------------------------------
            // Bounds
            //-----------------------------------------------------------------
#pragma region Math_Bounds
            /**
             * Computes the absolute value of a given number.
             *
             * @param A    The number for which to calculate the absolute value
             * @return     The absolute value of the input number
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Abs(const T A)
            {
                return A < static_cast<T>(0) ? -A : A;
            }

            /**
             * Returns the maximum of two values.
             *
             * @param A   First value to compare
             * @param B   Second value to compare
             * @return    The larger of the two input values
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Max(const T A, const T B)
            {
                return B < A ? A : B;
            }

            /**
             * Returns the smaller of two values.
             *
             * @param A   First value to compare
             * @param B   Second value to compare
             * @return    The smaller of the two input values
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Min(const T A, const T B)
            {
                return A < B ? A : B;
            }

            /**
             * Checks if value is within a range, exclusive on MaxValue.
             */
            template <class T, class U>
            [[nodiscard]] static constexpr FORCEINLINE bool IsWithin(const T& TestValue, const U& MinValue,
                                                                     const U& MaxValue)
            {
                return TestValue >= MinValue && TestValue < MaxValue;
            }

            /**
             * Checks if value is within a range, inclusive on MaxValue.
             */
            template <class T, class U>
            [[nodiscard]] static constexpr FORCEINLINE bool IsWithinInclusive(const T& TestValue, const U& MinValue,
                                                                              const U& MaxValue)
            {
                return TestValue >= MinValue && TestValue <= MaxValue;
            }

            /**
             *	Checks if two floating point numbers are nearly equal.
             *	
             *	@param A				First number to compare
             *	@param B				Second number to compare
             *	@param ErrorTolerance	Maximum allowed difference for considering them as 'nearly equal'
             *	@return					true if A and B are nearly equal
             */
            [[nodiscard]] static FORCEINLINE bool IsNearlyEqual(float A, float B,
                                                                float ErrorTolerance = OP_SMALL_NUMBER)
            {
                return Abs<float>(A - B) <= ErrorTolerance;
            }

            /**
             *	Checks if two double floating point numbers are nearly equal.
             *	
             *	@param A				First number to compare
             *	@param B				Second number to compare
             *	@param ErrorTolerance	Maximum allowed difference for considering them as 'nearly equal'
             *	@return					true if A and B are nearly equal
             */
            [[nodiscard]] static FORCEINLINE bool IsNearlyEqual(double A, double B,
                                                                double ErrorTolerance = OP_DOUBLE_SMALL_NUMBER)
            {
                return Abs<double>(A - B) <= ErrorTolerance;
            }

            /**
             *	Checks if a floating point number is nearly zero.
             *	
             *	@param Value			Number to compare
             *	@param ErrorTolerance	Maximum allowed difference for considering Value as 'nearly zero'
             *	@return					true if Value is nearly zero
             */
            [[nodiscard]] static FORCEINLINE bool IsNearlyZero(float Value, float ErrorTolerance = OP_SMALL_NUMBER)
            {
                return Abs<float>(Value) <= ErrorTolerance;
            }

            /**
             *	Checks if a double floating point number is nearly zero.
             *	
             *	@param Value			Number to compare
             *	@param ErrorTolerance	Maximum allowed difference for considering Value as 'nearly zero'
             *	@return					true if Value is nearly zero
             */
            [[nodiscard]] static FORCEINLINE bool IsNearlyZero(double Value,
                                                               double ErrorTolerance = OP_DOUBLE_SMALL_NUMBER)
            {
                return Abs<double>(Value) <= ErrorTolerance;
            }

            /**
             * Checks if the given number is a NaN (Not-a-Number).
             * Check Ogre 'IsNaN'
             *
             * @param F    Number to check if it is NaN
             * @return     true if F is NaN, false otherwise
             */
            template <typename T>
                requires TIsFloat(T)
            [[nodiscard]] static FORCEINLINE bool IsNaN(T F)
            {
                // std::isnan() is C99, not supported by all compilers
                // However NaN always fails this next test, no other number does.
                // ReSharper disable once CppIdenticalOperandsInBinaryExpression
                return F != F;
            }

            /**
             * Check whether the given float value is equal to zero.
             *
             * @param F The float value to be checked.
             * @return True if the value is equal to zero, false otherwise.
             */
            [[nodiscard]] static FORCEINLINE bool IsZero(float F)
            {
                return F == OP_ZERO; // NOLINT(clang-diagnostic-float-equal)
            }

            /**
             * Safely checks if the given floating-point number is close to zero within a defined tolerance.
             *
             * Imprecision in Representation: Numbers like 0.1 or 0.2 cannot be represented exactly in binary floating-point format, leading to small errors.
             * Rounding Errors: Operations on floating-point numbers can introduce rounding errors, accumulating over time.
             * Compiler Checks: Modern compilers and tools like Clang warn developers to avoid direct comparisons for floating-point values due to these issues.
             *
             * @param F The floating-point value to check.
             * @return True if the absolute value of F is less than the defined threshold, false otherwise.
             */
            [[nodiscard]] static FORCEINLINE bool IsZeroSafe(float F)
            {
                return Abs(F) < OP_EPSILON;
            }

            /**
             * Checks if the provided double value is equal to zero.
             *
             * @param F The double value to check.
             * @return True if the value is zero, otherwise false.
             */
            [[nodiscard]] static FORCEINLINE bool IsZero(double F)
            {
                return F == OP_DOUBLE_ZERO; // NOLINT(clang-diagnostic-float-equal)
            }

            /**
             * Checks if a given double value is safely non-zero by comparing it to a predefined epsilon.
             * 
             * Imprecision in Representation: Numbers like 0.1 or 0.2 cannot be represented exactly in binary floating-point format, leading to small errors.
             * Rounding Errors: Operations on floating-point numbers can introduce rounding errors, accumulating over time.
             * Compiler Checks: Modern compilers and tools like Clang warn developers to avoid direct comparisons for floating-point values due to these issues.
             * 
             * @param F The double value to check.
             * @return True if the absolute value of F is greater than the epsilon threshold, otherwise false.
             */
            [[nodiscard]] static FORCEINLINE bool IsZeroSafe(double F)
            {
                return Abs(F) < OP_DOUBLE_EPSILON;
            }

            /**
             * Clamps X to be between Min and Max, inclusive
             *
             * @param X         The value to clamp within the range
             * @param MinValue  The minimum value of the range
             * @param MaxValue  The maximum value of the range
             * @return          The clamped value within the specified range
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Clamp(const T X, const T MinValue, const T MaxValue)
            {
                return Max(Min(X, MaxValue), MinValue);
            }
#pragma endregion //Math_Bounds

            //-----------------------------------------------------------------
            // TRUNCATE
            //-----------------------------------------------------------------
#pragma region Math_Truncate
            /**
             * Converts a floating point number to 32-bit integer by truncating towards zero.
             *
             * @param F	The floating point number to convert
             * @return	The truncated 32-bit integer value of the input
             */
            [[nodiscard]] static constexpr FORCEINLINE Int32 TruncToInt32(float F) { return static_cast<Int32>(F); }

            /**
             * Converts a floating point number to a 32-bit integer by truncating towards zero.
             *
             * @param F    The floating point number to convert
             * @return     The truncated 32-bit integer value of the input
             */
            [[nodiscard]] static constexpr FORCEINLINE Int32 TruncToInt32(double F) { return static_cast<Int32>(F); }

            /**
             * Converts a floating point number to a 64-bit integer by truncating towards zero.
             *
             * @param F    The floating point number to convert
             * @return     The truncated 64-bit integer value of the input
             */
            [[nodiscard]] static constexpr FORCEINLINE Int64 TruncToInt64(double F) { return static_cast<Int64>(F); }


            /**
             * Converts a floating point number to a 32-bit integer by truncating towards zero.
             *
             * @param F    The floating point number to convert
             * @return     The truncated 32-bit integer value of the input
             */
            [[nodiscard]] static FORCEINLINE float TruncToFloat(float F) { return truncf(F); }

            /**
             * Converts a floating point number to a double by truncating towards zero.
             *
             * @param F    The floating point number to convert to double
             * @return     The truncated double value of the input
             */
            [[nodiscard]] static FORCEINLINE double TruncToDouble(double F) { return trunc(F); }
#pragma endregion //Math_Truncate

            //-----------------------------------------------------------------
            // FLOOR
            //-----------------------------------------------------------------
#pragma region Math_Floor
            /**
             * FLOOR a floating point number to a 32-bit integer by flooring towards negative infinity.
             *
             * @param F    The floating point number to convert
             * @return     The floored 32-bit integer value of the input
             */
            [[nodiscard]] static FORCEINLINE Int32 FloorToInt32(float F)
            {
                Int32 I = TruncToInt32(F);
                I -= static_cast<float>(I) > F;
                return I;
            }

            /**
             * FLOOR a double point number to a 32-bit integer by flooring towards negative infinity.
             *
             * @param F    The double point number to convert
             * @return     The floored 32-bit integer value of the input
             */
            [[nodiscard]] static FORCEINLINE Int32 FloorToInt32(double F)
            {
                Int32 I = TruncToInt32(F);
                I -= static_cast<double>(I) > F;
                return I;
            }

            /**
             * FLOOR a double point number to a 64-bit integer by flooring towards negative infinity.
             *
             * @param F       The double precision floating point number to round down
             * @return        The floored 64-bit integer value of the input
             */
            [[nodiscard]] static FORCEINLINE Int64 FloorToInt64(double F)
            {
                Int64 I = TruncToInt64(F);
                I -= static_cast<double>(I) > F;
                return I;
            }

            /**
             * FLOOR a floating point number to the nearest integer less than or equal to it as float.
             *
             * @param F  The input floating point number to round down
             * @return   The nearest integer less than or equal to the input number
             */
            [[nodiscard]] static FORCEINLINE float FloorToFloat(float F)
            {
                return floorf(F);
            }

            /**
             * FLOOR a double point number to the nearest integer less than or equal to it as double.
             *
             * @param F  The input floating point number to round down
             * @return   The nearest integer less than or equal to the input number
             */
            [[nodiscard]] static FORCEINLINE double FloorToDouble(double F)
            {
                return floor(F);
            }
#pragma endregion //Math_Floor

            //-----------------------------------------------------------------
            // ROUND
            //-----------------------------------------------------------------
#pragma region Math_Round
            /**
             * Rounds a float value to the nearest integer value as Int32.
             *
             * @param F     The float value to round to the nearest integer.
             * @return      Nearest integer value as Int32 to the input float value.
             */
            [[nodiscard]] static FORCEINLINE Int32 RoundToInt32(float F)
            {
                return FloorToInt32(F + 0.5f);
            }

            /**
             * Rounds a double value to the nearest integer value as Int32.
             *
             * @param F     The float value to round to the nearest integer.
             * @return      Nearest integer value as Int32 to the input float value.
             */
            [[nodiscard]] static FORCEINLINE Int32 RoundToInt32(double F)
            {
                return FloorToInt32(F + 0.5);
            }

            /**
             * Rounds a double value to the nearest integer value as Int64.
             *
             * @param F     The float value to round to the nearest integer.
             * @return      Nearest integer value as Int64 to the input float value.
             */
            [[nodiscard]] static FORCEINLINE Int64 RoundToInt64(double F)
            {
                return FloorToInt64(F + 0.5);
            }

            /**
             * Rounds a float value to the nearest integer value as float.
             *
             * @param F     The float value to round to the nearest integer.
             * @return      Nearest integer value as float to the input float value.
             */
            [[nodiscard]] static FORCEINLINE float RoundToFloat(float F)
            {
                return FloorToFloat(F + 0.5f);
            }

            /**
             * Rounds a double value to the nearest integer value as double.
             *
             * @param F     The float value to round to the nearest integer.
             * @return      Nearest integer value as double to the input float value.
             */
            [[nodiscard]] static FORCEINLINE double RoundToDouble(double F)
            {
                return FloorToDouble(F + 0.5);
            }
#pragma endregion //Math_Round

            //-----------------------------------------------------------------
            // CEIL
            //-----------------------------------------------------------------
#pragma region Math_Ceil
            /**
             * Round up integer value greater than or equal to the specified float number.
             *
             * @param F	The float number to find the ceiling integer value of
             * @return		The ceiling int-32 value of the input float number
             */
            [[nodiscard]] static FORCEINLINE Int32 CeilToInt32(float F)
            {
                Int32 I = TruncToInt32(F);
                I += static_cast<float>(I) < F;
                return I;
            }

            /**
             * Round up integer value greater than or equal to the specified double number.
             *
             * @param F	The double number to find the ceiling integer value of
             * @return		The ceiling int-32 value of the input double number
             */
            [[nodiscard]] static FORCEINLINE Int32 CeilToInt32(double F)
            {
                Int32 I = TruncToInt32(F);
                I += static_cast<double>(I) < F;
                return I;
            }

            /**
             * Round up integer value greater than or equal to the specified double number.
             *
             * @param F	The double number to find the ceiling integer value of
             * @return		The ceiling int-64 value of the input double number
             */
            [[nodiscard]] static FORCEINLINE Int64 CeilToInt64(double F)
            {
                Int64 I = TruncToInt64(F);
                I += static_cast<double>(I) < F;
                return I;
            }

            /**
             * Round up integer value greater than or equal to the specified float number as float.
             *
             * @param F	The float number to find the ceiling integer value of
             * @return	The ceiling float value of the input float number
             */
            [[nodiscard]] static FORCEINLINE float CeilToFloat(float F)
            {
                return ceilf(F);
            }

            /**
             * Round up integer value greater than or equal to the specified double number as double.
             *
             * @param F	The float number to find the ceiling integer value of
             * @return	The ceiling double value of the input double number
            */
            [[nodiscard]] static FORCEINLINE double CeilToDouble(double F)
            {
                return ceil(F);
            }
#pragma endregion //Math_Ceil

            //-----------------------------------------------------------------
            // SOH CAH TOA
            //
            // SOH = Sine is Opposite over Hypotenuse.      (Opposite / Hypotenuse)
            // CAH = Cosine is Adjacent over Hypotenuse.    (Adjacent / Hypotenuse)
            // TOA = Tangent is Opposite over Adjacent.     (Opposite / Adjacent)
            //-----------------------------------------------------------------
#pragma region Math_SOHCAHTOA
            /**
             * Calculates the sine of a given floating point number.
             *
             * @param Value     The input value in radians
             * @return          The sine of the input value
             */
            [[nodiscard]] static FORCEINLINE float Sin(float Value) { return sinf(Value); }

            /**
             *	Computes the sine of the specified value.
             *
             *	@param Value	The input value in radians
             *	@return		    The sine of the input value
             */
            [[nodiscard]] static FORCEINLINE double Sin(double Value) { return sin(Value); }

            /**
             * Calculates the arcsine of a given floating point number.
             *
             * @param Value     The input value for which to calculate the arcsine.
             * @return          The arcsine value in radians.
             */
            [[nodiscard]] static FORCEINLINE float Asin(float Value)
            {
                return asinf(Value < -1.f ? -1.f : Value < 1.f ? Value : 1.f);
            }

            /**
             *  Calculates the arcsine of the given value.
             *
             *  @param Value   The input value for which the arcsine is to be calculated
             *  @return        The arcsine value in radians
             */
            [[nodiscard]] static FORCEINLINE double Asin(double Value)
            {
                return asin(Value < -1.0 ? -1.0 : Value < 1.0 ? Value : 1.0);
            }

            /**
             *	Calculates the hyperbolic sine of a specified value.
             *
             *	@param Value    The value for which to calculate the hyperbolic sine
             *	@return		    The hyperbolic sine of the input value
             */
            [[nodiscard]] static FORCEINLINE float Sinh(float Value) { return sinhf(Value); }

            /**
             *	Calculates the hyperbolic sine of a specified value.
             *
             *	@param Value    The value for which to calculate the hyperbolic sine
             *	@return		    The hyperbolic sine of the input value
             */
            [[nodiscard]] static FORCEINLINE double Sinh(double Value) { return sinh(Value); }

            /**
             *  Calculates the cosine of the given value.
             *
             *  @param Value    The input value in radians for which to calculate the cosine.
             *  @return         The cosine of the input value.
             */
            [[nodiscard]] static FORCEINLINE float Cos(float Value) { return cosf(Value); }

            /**
             *  Calculates the cosine of the given value.
             *
             *  @param Value    The input value in radians for which to calculate the cosine.
             *  @return         The cosine of the input value.
             */
            [[nodiscard]] static FORCEINLINE double Cos(double Value) { return cos(Value); }

            /**
             * Calculate the arc cosine of a given value.
             *
             * @param Value     The value for which to calculate the arc cosine
             * @return          The arc cosine value in radians
             */
            [[nodiscard]] static FORCEINLINE float Acos(float Value)
            {
                return acosf(Value < -1.f ? -1.f : Value < 1.f ? Value : 1.f);
            }

            /**
             * Calculate the arc cosine of a given value.
             *
             * @param Value     The value for which to calculate the arc cosine
             * @return          The arc cosine value in radians
             */
            [[nodiscard]] static FORCEINLINE double Acos(double Value)
            {
                return acos(Value < -1.0 ? -1.0 : Value < 1.0 ? Value : 1.0);
            }

            /**
             * Calculates the hyperbolic cosine of the specified value.
             *
             * @param Value  The value for which to calculate the hyperbolic cosine
             * @return       The hyperbolic cosine of the given value
             */
            [[nodiscard]] static FORCEINLINE float Cosh(float Value) { return coshf(Value); }

            /**
             * Calculates the hyperbolic cosine of the specified value.
             *
             * @param Value  The value for which to calculate the hyperbolic cosine
             * @return       The hyperbolic cosine of the given value
             */
            [[nodiscard]] static FORCEINLINE double Cosh(double Value) { return cosh(Value); }

            /**
             * Calculates the tangent of a given angle in radians.
             *
             * @param Value     The angle in radians for which to calculate the tangent
             * @return          The tangent of the given angle
             */
            [[nodiscard]] static FORCEINLINE float Tan(float Value) { return tanf(Value); }

            /**
             * Calculates the tangent of a given angle in radians.
             *
             * @param Value     The angle in radians for which to calculate the tangent
             * @return          The tangent of the given angle
             */
            [[nodiscard]] static FORCEINLINE double Tan(double Value) { return tan(Value); }

            /**
             * Calculates the arctangent of a specified value.
             *
             * @param Value     The value for which to calculate the arctangent
             * @return          The arctangent value in radians
             */
            [[nodiscard]] static FORCEINLINE float Atan(float Value) { return atanf(Value); }

            /**
             * Calculates the arctangent of a specified value.
             *
             * @param Value     The value for which to calculate the arctangent
             * @return          The arctangent value in radians
             */
            [[nodiscard]] static FORCEINLINE double Atan(double Value) { return atan(Value); }

            /**
             *	Returns the hyperbolic tangent of a floating point number.
             *
             *	@param Value	The input value for calculating hyperbolic tangent
             *	@return			The hyperbolic tangent of the input value
             */
            [[nodiscard]] static FORCEINLINE float Tanh(float Value) { return tanhf(Value); }

            /**
             *	Returns the hyperbolic tangent of a floating point number.
             *
             *	@param Value	The input value for calculating hyperbolic tangent
             *	@return			The hyperbolic tangent of the input value
             */
            [[nodiscard]] static FORCEINLINE double Tanh(double Value) { return tanh(Value); }

            /**
             * Calculates the arctangent of the quotient of two specified numbers.
             * Unreal version.
             * @param Y The numerator
             * @param X The denominator
             * @return The angle in radians whose tangent is the quotient of Y and X
             */
            [[nodiscard]] static float Atan2(float Y, float X);

            /**
             * Calculates the arctangent of the quotient of two specified numbers.
             * Unreal version.
             * @param Y The numerator
             * @param X The denominator
             * @return The angle in radians whose tangent is the quotient of Y and X
             */
            [[nodiscard]] static double Atan2(double Y, double X);
#pragma endregion //Math_SOHCAHTOA

            //-----------------------------------------------------------------
            // COMPUTE
            //-----------------------------------------------------------------
#pragma region Math_COMPUTE
            /**
             * Calculates the square root of a given floating point number.
             *
             * @param Value The number for which the square root is to be calculated
             * @return      The square root of the input Value
             */
            [[nodiscard]] static FORCEINLINE float Sqrt(float Value) { return sqrtf(Value); }

            /**
             * Calculates the square root of a given double point number.
             *
             * @param Value The number for which the square root is to be calculated
             * @return      The square root of the input Value
             */
            [[nodiscard]] static FORCEINLINE double Sqrt(double Value) { return sqrt(Value); }

            /**
             * Calculate the inverse square root of the given value.
             *
             * Using SSE instructions makes the computation faster because:
             *      - _mm_sqrt_ss() is optimized at the CPU level.
             *      - No branching (unlike Newton's approximation in 1.0f / sqrt(x)).
             *      - Can be easily parallelized for vectorized operations.
             *
             * @param InValue The input value for which the inverse square root will be calculated.
             * @return The inverse square root of the input value.
             */
            [[nodiscard]] static float InvSqrt(float InValue);

            /**
             * Calculate the inverse square root of the given value.
             *
             * Using SSE instructions makes the computation faster because:
             *      - _mm_sqrt_ss() is optimized at the CPU level.
             *      - No branching (unlike Newton's approximation in 1.0f / sqrt(x)).
             *      - Can be easily parallelized for vectorized operations.
             *
             * @param InValue The input value for which the inverse square root will be calculated.
             * @return The inverse square root of the input value.
             */
            [[nodiscard]] static double InvSqrt(double InValue);

            /**
             * Computes the inverse square root of a given floating-point value using an approximation method.
             * The Quake trick uses bitwise hacks and Newton's iteration.
             *
             * Use @see InvSqrt, but it fun to remind the Quake Trick using 0x5f3759df.
             * 
             * @param InValue The floating-point value for which the inverse square root is calculated.
             *                It must be greater than zero.
             * @return The approximated inverse square root of the given value.
             */
            [[nodiscard]] static float QuakeInvSqrt(float InValue);

            /**
             *	Calculates the square of a given value.
             *
             *	@param Value    The value for which the square will be calculated
             *	@return         The square of the input value
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Square(const T Value) { return Value * Value; }

            /**
             * Calculates the cube of a given value.
             *
             * @param Value    The value whose cube is to be calculated
             * @return         The cube of the input value
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Cube(const T Value) { return Value * Value * Value; }

            /**
             * Calculates the 4th power of the given value.
             *
             * @param Value     The input value to be raised to the 4th power
             * @return          The 4th power of the input value
             */
            template <class T>
            [[nodiscard]] static constexpr FORCEINLINE T Quad(const T Value) { return Value * Value * Value * Value; }

            /**
             * Calculates the power of a given number to another number.
             *
             * @param A The base number.
             * @param B The exponent.
             * @return The result of A raised to the power of B.
             */
            [[nodiscard]] static FORCEINLINE float Pow(float A, float B) { return powf(A, B); }

            /**
             * Calculates the power of a given number to another number.
             *
             * @param A The base number.
             * @param B The exponent.
             * @return The result of A raised to the power of B.
             */
            [[nodiscard]] static FORCEINLINE double Pow(double A, double B) { return pow(A, B); }

            /**
             *	Performs a linear interpolation between two values, Alpha ranges from 0-1
             *
             *	@param A     The start value for interpolation
             *	@param B     The end value for interpolation
             *	@param Alpha The interpolation factor (usually between 0.0 and 1.0)
             *	@return      The interpolated value between A and B at the given Alpha factor
             */
            template <typename T, typename U, TEnebleIf(TIsFloat(U) || TIsSameAs(T, U) && TIsSameAs(T, bool))>
            [[nodiscard]] static constexpr FORCEINLINE T Lerp(const T& A, const T& B, const U& Alpha)
            {
                return static_cast<T>(A + Alpha * (B - A));
            }

            /**
             * Linearly interpolates between two values A and B by the specified alpha value.
             * A and B can be different type as long is not a bool.
             * 
             * @param A      The starting value
             * @param B      The ending value
             * @param Alpha  The interpolation factor between A and B (0.0 for A, 1.0 for B)
             * @return       The interpolated value between A and B based on Alpha
             */
            template <typename T1, typename T2, typename T3, TEnebleIf(
                          TIsFloat(T3) && !TIsSameAs(T1, bool) && !TIsSameAs(T2, bool))>
            [[nodiscard]] static auto Lerp(const T1 A, const T2 B, const T3 Alpha) -> decltype(A * B)
            {
                using ABType = decltype(A * B);
                return Lerp(ABType(A), ABType(B), Alpha);
            }
#pragma endregion //Math_COMPUTE

         //-----------------------------------------------------------------
         // ANGLE
         //-----------------------------------------------------------------
#pragma region Math_Angle

         /**
          * Converts an angle given in radians to degrees.
          *
          * @param Angle    The angle in radians to be converted to degrees
          * @return         The equivalent angle in degrees
          */
         [[nodiscard]] static float RadiansToDegrees(float Angle) { return Angle * OP_HALF_CIRCLE / OP_PI; }
    
         /**
          * Converts an angle given in degrees to radians.
          *
          * @param Angle     The angle in degrees to be converted to radians
          * @return          The angle converted to radians
          */
         [[nodiscard]] static float DegreesToRadians(float Angle) { return Angle * OP_PI / OP_HALF_CIRCLE; }

         /**
          *	Converts an angle given in radians to degrees.
          *
          *	@param Angle		The angle in radians to be converted to degrees
          *	@return				   The angle converted to degrees
          */
         [[nodiscard]] static double RadiansToDegrees(double Angle) { return Angle * OP_DOUBLE_HALF_CIRCLE / OP_DOUBLE_PI; }
    
         /**
          *	Converts an angle in degrees to radians.
          *
          *	@param Angle		The angle in degrees to convert to radians
          *	@return				   The corresponding angle in radians
          */
         [[nodiscard]] static double DegreesToRadians(double Angle) { return Angle * OP_DOUBLE_PI / OP_DOUBLE_HALF_CIRCLE; }

#pragma endregion //Math_Angle
        };
    }
}
