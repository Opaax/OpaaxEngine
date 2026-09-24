#pragma once

#include <Core/OpaaxTRequire.hpp>
#include "Core/EngineAPI.h"
#include "MathsStatics.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    struct OPAAX_API Maths
    {
        // =============================================================================
        // Bounds
        /**
        * Computes the absolute value of a given number.
        *
        * @param A    The number for which to calculate the absolute value
        * @return     The absolute value of the input number
        */
        template <CONCEPT_TIsFloatOrIntegral T>
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
        template <CONCEPT_TIsFloatOrIntegral T>
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
        template <CONCEPT_TIsFloatOrIntegral T>
        [[nodiscard]] static constexpr FORCEINLINE T Min(const T A, const T B)
        {
            return A < B ? A : B;
        }

        /**
         * Checks if value is within a range, exclusive on MaxValue.
         */
        template <CONCEPT_TIsFloatOrIntegral T, CONCEPT_TIsFloatOrIntegral U>
        [[nodiscard]] static constexpr FORCEINLINE bool IsWithin(const T& TestValue, const U& MinValue, const U& MaxValue)
        {
            return TestValue >= MinValue && TestValue < MaxValue;
        }

        /**
         * Checks if value is within a range, inclusive on MaxValue.
         */
        template <CONCEPT_TIsFloatOrIntegral T, CONCEPT_TIsFloatOrIntegral U>
        [[nodiscard]] static constexpr FORCEINLINE bool IsWithinInclusive(const T& TestValue, const U& MinValue, const U& MaxValue)
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
        [[nodiscard]] static FORCEINLINE bool IsNearlyEqual(float A, float B, float ErrorTolerance = FSMALL_NUMBER)
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
        [[nodiscard]] static FORCEINLINE bool IsNearlyEqual(double A, double B, double ErrorTolerance = DSMALL_NUMBER)
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
        [[nodiscard]] static FORCEINLINE bool IsNearlyZero(float Value, float ErrorTolerance = FSMALL_NUMBER)
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
        [[nodiscard]] static FORCEINLINE bool IsNearlyZero(double Value, double ErrorTolerance = DSMALL_NUMBER)
        {
            return Abs<double>(Value) <= ErrorTolerance;
        }

        /**
         * Checks if the given number is a NaN (Not-a-Number).
         * Check Ogre 'IsNaN'
         *
         * @param Value    Number to check if it is NaN
         * @return     true if F is NaN, false otherwise
         */
        template <CONCEPT_TIsFloat T>
        [[nodiscard]] static FORCEINLINE bool IsNaN(T Value)
        {
            // std::isnan() is C99, not supported by all compilers
            // However NaN always fails this next test, no other number does.
            // ReSharper disable once CppIdenticalOperandsInBinaryExpression
            return Value != Value;
        }

        /**
         * Check whether the given float value is equal to zero.
         *
         * @param Value The float value to be checked.
         * @return True if the value is equal to zero, false otherwise.
         */
        [[nodiscard]] static FORCEINLINE bool IsZero(float Value)
        {
            return Value == FZERO; // NOLINT(clang-diagnostic-float-equal)
        }

        /**
         * Safely checks if the given floating-point number is close to zero within a defined tolerance.
         *
         * Imprecision in Representation: Numbers like 0.1 or 0.2 cannot be represented exactly in binary floating-point format, leading to small errors.
         * Rounding Errors: Operations on floating-point numbers can introduce rounding errors, accumulating over time.
         * Compiler Checks: Modern compilers and tools like Clang warn developers to avoid direct comparisons for floating-point values due to these issues.
         *
         * @param Value The floating-point value to check.
         * @return True if the absolute value of F is less than the defined threshold, false otherwise.
         */
        [[nodiscard]] static FORCEINLINE bool IsZeroSafe(float Value)
        {
            return Abs(Value) < FEPSILON;
        }

        /**
         * Checks if the provided double value is equal to zero.
         *
         * @param Value The double value to check.
         * @return True if the value is zero, otherwise false.
         */
        [[nodiscard]] static FORCEINLINE bool IsZero(double Value)
        {
            return Value == DZERO; // NOLINT(clang-diagnostic-float-equal)
        }

        /**
         * Checks if a given double value is safely non-zero by comparing it to a predefined epsilon.
         * 
         * Imprecision in Representation: Numbers like 0.1 or 0.2 cannot be represented exactly in binary floating-point format, leading to small errors.
         * Rounding Errors: Operations on floating-point numbers can introduce rounding errors, accumulating over time.
         * Compiler Checks: Modern compilers and tools like Clang warn developers to avoid direct comparisons for floating-point values due to these issues.
         * 
         * @param Value The double value to check.
         * @return True if the absolute value of F is greater than the epsilon threshold, otherwise false.
         */
        [[nodiscard]] static FORCEINLINE bool IsZeroSafe(double Value)
        {
            return Abs(Value) < DEPSILON;
        }

        /**
         * Clamps X to be between Min and Max, inclusive
         *
         * @param X         The value to clamp within the range
         * @param MinValue  The minimum value of the range
         * @param MaxValue  The maximum value of the range
         * @return          The clamped value within the specified range
         */
        template <CONCEPT_TIsFloatOrIntegral T>
        [[nodiscard]] static constexpr FORCEINLINE T Clamp(const T X, const T MinValue, const T MaxValue)
        {
            return Max(Min(X, MaxValue), MinValue);
        }

        // End Bounds
        // =============================================================================
        
        // =============================================================================
        // Truncate

        /**
         * Converts a floating point number to 32-bit integer by truncating towards zero.
         *
         * @param Value	The floating point number to convert
         * @return	The truncated 32-bit integer value of the input
         */
        [[nodiscard]] static constexpr FORCEINLINE Int32 TruncToInt32(float Value) { return static_cast<Int32>(Value); }

        /**
         * Converts a floating point number to a 32-bit integer by truncating towards zero.
         *
         * @param Value    The floating point number to convert
         * @return     The truncated 32-bit integer value of the input
         */
        [[nodiscard]] static constexpr FORCEINLINE Int32 TruncToInt32(double Value) { return static_cast<Int32>(Value); }

        /**
         * Converts a floating point number to a 64-bit integer by truncating towards zero.
         *
         * @param Value    The floating point number to convert
         * @return     The truncated 64-bit integer value of the input
         */
        [[nodiscard]] static constexpr FORCEINLINE Int64 TruncToInt64(double Value) { return static_cast<Int64>(Value); }


        /**
         * Converts a floating point number to a 32-bit integer by truncating towards zero.
         *
         * @param Value    The floating point number to convert
         * @return     The truncated 32-bit integer value of the input
         */
        [[nodiscard]] static FORCEINLINE float TruncToFloat(float Value) { return truncf(Value); }

        /**
         * Converts a floating point number to a double by truncating towards zero.
         *
         * @param Value    The floating point number to convert to double
         * @return     The truncated double value of the input
         */
        [[nodiscard]] static FORCEINLINE double TruncToDouble(double Value) { return trunc(Value); }
        
        // End Truncate
        // =============================================================================
        
        // =============================================================================
        // Floor
        /**
         * FLOOR a floating point number to a 32-bit integer by flooring towards negative infinity.
         *
         * @param Value    The floating point number to convert
         * @return     The floored 32-bit integer value of the input
         */
        [[nodiscard]] static FORCEINLINE Int32 FloorToInt32(float Value)
        {
            Int32 I = TruncToInt32(Value);
            I -= static_cast<float>(I) > Value;
            return I;
        }

        /**
         * FLOOR a double point number to a 32-bit integer by flooring towards negative infinity.
         *
         * @param Value    The double point number to convert
         * @return     The floored 32-bit integer value of the input
         */
        [[nodiscard]] static FORCEINLINE Int32 FloorToInt32(double Value)
        {
            Int32 I = TruncToInt32(Value);
            I -= static_cast<double>(I) > Value;
            return I;
        }

        /**
         * FLOOR a double point number to a 64-bit integer by flooring towards negative infinity.
         *
         * @param Value       The double precision floating point number to round down
         * @return        The floored 64-bit integer value of the input
         */
        [[nodiscard]] static FORCEINLINE Int64 FloorToInt64(double Value)
        {
            Int64 I = TruncToInt64(Value);
            I -= static_cast<double>(I) > Value;
            return I;
        }

        /**
         * FLOOR a floating point number to the nearest integer less than or equal to it as float.
         *
         * @param Value  The input floating point number to round down
         * @return   The nearest integer less than or equal to the input number
         */
        [[nodiscard]] static FORCEINLINE float FloorToFloat(float Value)
        {
            return floorf(Value);
        }

        /**
         * FLOOR a double point number to the nearest integer less than or equal to it as double.
         *
         * @param Value  The input floating point number to round down
         * @return   The nearest integer less than or equal to the input number
         */
        [[nodiscard]] static FORCEINLINE double FloorToDouble(double Value)
        {
            return floor(Value);
        }
        // End Floor
        // =============================================================================
        
        // =============================================================================
        // Round
        /**
         * Rounds a float value to the nearest integer value as Int32.
         *
         * @param Value     The float value to round to the nearest integer.
         * @return          Nearest integer value as Int32 to the input float value.
         */
        [[nodiscard]] static FORCEINLINE Int32 RoundToInt32(float Value)
        {
            return FloorToInt32(Value + 0.5f);
        }

        /**
         * Rounds a double value to the nearest integer value as Int32.
         *
         * @param Value     The float value to round to the nearest integer.
         * @return          Nearest integer value as Int32 to the input float value.
         */
        [[nodiscard]] static FORCEINLINE Int32 RoundToInt32(double Value)
        {
            return FloorToInt32(Value + 0.5);
        }

        /**
         * Rounds a double value to the nearest integer value as Int64.
         *
         * @param Value     The float value to round to the nearest integer.
         * @return          Nearest integer value as Int64 to the input float value.
         */
        [[nodiscard]] static FORCEINLINE Int64 RoundToInt64(double Value)
        {
            return FloorToInt64(Value + 0.5);
        }

        /**
         * Rounds a float value to the nearest integer value as float.
         *
         * @param Value     The float value to round to the nearest integer.
         * @return          Nearest integer value as float to the input float value.
         */
        [[nodiscard]] static FORCEINLINE float RoundToFloat(float Value)
        {
            return FloorToFloat(Value + 0.5f);
        }

        /**
         * Rounds a double value to the nearest integer value as double.
         *
         * @param Value     The float value to round to the nearest integer.
         * @return          Nearest integer value as double to the input float value.
         */
        [[nodiscard]] static FORCEINLINE double RoundToDouble(double Value)
        {
            return FloorToDouble(Value + 0.5);
        }
        // End Round
        // =============================================================================
        
        // =============================================================================
        // Ceil
        /**
         * Round up integer value greater than or equal to the specified float number.
         *
         * @param Value	The float number to find the ceiling integer value of
         * @return		The ceiling int-32 value of the input float number
         */
        [[nodiscard]] static FORCEINLINE Int32 CeilToInt32(float Value)
        {
            Int32 I = TruncToInt32(Value);
            I += static_cast<float>(I) < Value;
            return I;
        }

        /**
         * Round up integer value greater than or equal to the specified double number.
         *
         * @param Value	The double number to find the ceiling integer value of
         * @return		The ceiling int-32 value of the input double number
         */
        [[nodiscard]] static FORCEINLINE Int32 CeilToInt32(double Value)
        {
            Int32 I = TruncToInt32(Value);
            I += static_cast<double>(I) < Value;
            return I;
        }

        /**
         * Round up integer value greater than or equal to the specified double number.
         *
         * @param Value	The double number to find the ceiling integer value of
         * @return		The ceiling int-64 value of the input double number
         */
        [[nodiscard]] static FORCEINLINE Int64 CeilToInt64(double Value)
        {
            Int64 I = TruncToInt64(Value);
            I += static_cast<double>(I) < Value;
            return I;
        }

        /**
         * Round up integer value greater than or equal to the specified float number as float.
         *
         * @param Value	The float number to find the ceiling integer value of
         * @return	The ceiling float value of the input float number
         */
        [[nodiscard]] static FORCEINLINE float CeilToFloat(float Value)
        {
            return ceilf(Value);
        }

        /**
         * Round up integer value greater than or equal to the specified double number as double.
         *
         * @param Value	The float number to find the ceiling integer value of
         * @return	The ceiling double value of the input double number
        */
        [[nodiscard]] static FORCEINLINE double CeilToDouble(double Value)
        {
            return ceil(Value);
        }
        // End Ceil
        // =============================================================================
        
        // =============================================================================
        // SOH CAH TOA
        
        //-----------------------------------------------------------------
        // SOH CAH TOA
        //
        // SOH = Sine is Opposite over Hypotenuse.      (Opposite / Hypotenuse)
        // CAH = Cosine is Adjacent over Hypotenuse.    (Adjacent / Hypotenuse)
        // TOA = Tangent is Opposite over Adjacent.     (Opposite / Adjacent)
        //-----------------------------------------------------------------

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
        [[nodiscard]] static FORCEINLINE float ASin(float Value)
        {
            return asinf(Value < -1.f ? -1.f : Value < 1.f ? Value : 1.f);
        }

        /**
         *  Calculates the arcsine of the given value.
         *
         *  @param Value   The input value for which the arcsine is to be calculated
         *  @return        The arcsine value in radians
         */
        [[nodiscard]] static FORCEINLINE double ASin(double Value)
        {
            return asin(Value < -1.0 ? -1.0 : Value < 1.0 ? Value : 1.0);
        }

        /**
         *	Calculates the hyperbolic sine of a specified value.
         *
         *	@param Value    The value for which to calculate the hyperbolic sine
         *	@return		    The hyperbolic sine of the input value
         */
        [[nodiscard]] static FORCEINLINE float SinHyperbolic(float Value) { return sinhf(Value); }

        /**
         *	Calculates the hyperbolic sine of a specified value.
         *
         *	@param Value    The value for which to calculate the hyperbolic sine
         *	@return		    The hyperbolic sine of the input value
         */
        [[nodiscard]] static FORCEINLINE double SinHyperbolic(double Value) { return sinh(Value); }

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
        [[nodiscard]] static FORCEINLINE float ACos(float Value)
        {
            return acosf(Value < -1.f ? -1.f : Value < 1.f ? Value : 1.f);
        }

        /**
         * Calculate the arc cosine of a given value.
         *
         * @param Value     The value for which to calculate the arc cosine
         * @return          The arc cosine value in radians
         */
        [[nodiscard]] static FORCEINLINE double ACos(double Value)
        {
            return acos(Value < -1.0 ? -1.0 : Value < 1.0 ? Value : 1.0);
        }

        /**
         * Calculates the hyperbolic cosine of the specified value.
         *
         * @param Value  The value for which to calculate the hyperbolic cosine
         * @return       The hyperbolic cosine of the given value
         */
        [[nodiscard]] static FORCEINLINE float CosHyperbolic(float Value) { return coshf(Value); }

        /**
         * Calculates the hyperbolic cosine of the specified value.
         *
         * @param Value  The value for which to calculate the hyperbolic cosine
         * @return       The hyperbolic cosine of the given value
         */
        [[nodiscard]] static FORCEINLINE double CosHyperbolic(double Value) { return cosh(Value); }

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
        [[nodiscard]] static FORCEINLINE float ATan(float Value) { return atanf(Value); }

        /**
         * Calculates the arctangent of a specified value.
         *
         * @param Value     The value for which to calculate the arctangent
         * @return          The arctangent value in radians
         */
        [[nodiscard]] static FORCEINLINE double ATan(double Value) { return atan(Value); }

        /**
         *	Returns the hyperbolic tangent of a floating point number.
         *
         *	@param Value	The input value for calculating hyperbolic tangent
         *	@return			The hyperbolic tangent of the input value
         */
        [[nodiscard]] static FORCEINLINE float TanHyperbolic(float Value) { return tanhf(Value); }

        /**
         *	Returns the hyperbolic tangent of a floating point number.
         *
         *	@param Value	The input value for calculating hyperbolic tangent
         *	@return			The hyperbolic tangent of the input value
         */
        [[nodiscard]] static FORCEINLINE double TanHyperbolic(double Value) { return tanh(Value); }

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
        
        // End SOH CAH TOA
        // =============================================================================
        
        // =============================================================================
        // Compute
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
        [[nodiscard]] static float InvSqrt(float InValue) { return FONE / sqrtf(InValue); }

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
        [[nodiscard]] static double InvSqrt(double InValue) { return DONE / sqrt(InValue); }

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
        template <typename T, typename U, TEnebleIf(TIsFloat(U)|| TIsSameAs(T, U) && TIsSameAs(T, bool))>
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
        template <typename T1, typename T2, typename T3, TEnebleIf(TIsFloat(T3) &&!TIsSameAs(T1, bool) &&!TIsSameAs(T2, bool))>
        [[nodiscard]] static auto Lerp(const T1 A, const T2 B, const T3 Alpha) -> decltype(A * B)
        {
            using ABType = decltype(A * B);
            return Lerp(ABType(A), ABType(B), Alpha);
        }

        /**
         * 
         * @param Value 
         * @return e^Value
         */
        [[nodiscard]] static FORCEINLINE float  Exp( float Value )  { return expf(Value);   }

        /**
         * 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE double Exp(double Value)   { return exp(Value);    }

        /**
         * 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE float  Exp2( float Value ) { return powf(2.f, Value); /*exp2f(Value);*/ }
        [[nodiscard]] static FORCEINLINE double Exp2(double Value)  { return pow(2.0, Value); /*exp2(Value);*/ }

        /**
         * 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE float Logarithm( float Value ) { return logf(Value); }

        /**
         * 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE double Logarithm(double Value) { return log(Value); }

        /**
         * 
         * @param Base 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE float LogX( float Base, float Value ) { return Logarithm(Value) / Logarithm(Base); }

        /**
         * 
         * @param Base 
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE double LogX(double Base, double Value) { return Logarithm(Value) / Logarithm(Base); }
        
        /**
         * 1.0 / Logarithm(2) = 1.442695040888963387
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE float Log2( float Value ) { return Logarithm(Value) * 1.4426950f; }	
        
        /**
         * 1.0 / Logarithm(2) = 1.442695040888963387
         * @param Value 
         * @return 
         */
        [[nodiscard]] static FORCEINLINE double Log2(double Value) { return Logarithm(Value) * 1.442695040888963387; }
        
        
        /**
        * Breaks the given value into an integral and a fractional part.
        * @param InValue	Floating point value to convert
        * @param OutIntPart Floating point value that receives the integral part of the number.
        * @return			The fractional part of the number.
        */
        [[nodiscard]] static FORCEINLINE float Modf(const float InValue, float* OutIntPart)
        {
            return modff(InValue, OutIntPart);
        }

        /**
        * Breaks the given value into an integral and a fractional part.
        * @param InValue	Floating point value to convert
        * @param OutIntPart Floating point value that receives the integral part of the number.
        * @return			The fractional part of the number.
        */
        [[nodiscard]] static FORCEINLINE double Modf(const double InValue, double* OutIntPart)
        {
            return modf(InValue, OutIntPart);
        }
        
        /**
 * Returns the floating-point remainder of X / Y
 * Warning: Always returns remainder toward 0, not toward the smaller multiple of Y.
 *			So for example Fmod(2.8f, 2) gives .8f as you would expect, however, Fmod(-2.8f, 2) gives -.8f, NOT 1.2f
 * Use Floor instead when snapping positions that can be negative to a grid
 *
 * This is forced to *NOT* inline so that divisions by constant Y does not get optimized in to an inverse scalar multiply,
 * which is not consistent with the intent nor with the vectorized version.
 */

        /**
        * Warning: Always returns remainder toward 0, not toward the smaller multiple of Y.
        * So for example Fmod(2.8f, 2) gives .8f as you would expect, however, Fmod(-2.8f, 2) gives -.8f, NOT 1.2f
        * @return the floating-point remainder of X / Y
        */
        [[nodiscard]] static float  FMod(float X, float Y);

        /**
         * Warning: Always returns remainder toward 0, not toward the smaller multiple of Y.
         * So for example Fmod(2.8f, 2) gives .8f as you would expect, however, Fmod(-2.8f, 2) gives -.8f, NOT 1.2f
         * @param X 
         * @param Y 
         * @return the floating-point remainder of X / Y
         */
        [[nodiscard]] static double FMod(double X, double Y);
        
        // End Compute
        // =============================================================================
        
        // =============================================================================
        // Conversion
        
        // =============================================================================
        // Angle
        /**
         * Converts an angle given in radians to degrees.
         *
         * @param Radian    The angle in radians to be converted to degrees
         * @return          The equivalent angle in degrees
         */
        [[nodiscard]] static float RadiansToDegrees(float Radian) { return Radian * FHALF_CIRCLE_DEGREE / FPI; }

        /**
         * Converts an angle given in degrees to radians.
         *
         * @param Angle     The angle in degrees to be converted to radians
         * @return          The angle converted to radians
         */
        [[nodiscard]] static float DegreesToRadians(float Angle) { return Angle * FPI / FHALF_CIRCLE_DEGREE; }

        /**
         *	Converts an angle given in radians to degrees.
         *
         *	@param Radian		The angle in radians to be converted to degrees
         *	@return				The angle converted to degrees
         */
        [[nodiscard]] static double RadiansToDegrees(double Radian)
        {
            return Radian * DHALF_CIRCLE_DEGREE / DPI;
        }

        /**
         *	Converts an angle in degrees to radians.
         *
         *	@param Angle		The angle in degrees to convert to radians
         *	@return				The corresponding angle in radians
         */
        [[nodiscard]] static double DegreesToRadians(double Angle)
        {
            return Angle * DPI / DHALF_CIRCLE_DEGREE;
        }
        // End Angle
        // =============================================================================
        
        // End Conversion
        // =============================================================================
    };
}
