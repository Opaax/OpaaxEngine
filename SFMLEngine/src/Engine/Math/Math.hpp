#pragma once
#include <random>

#include "../EngineType.hpp"

namespace StaticMath
{
    inline extern const double PI                   {3.1415926535897932f}; /* Extra digits if needed: 3.1415926535897932384626433832795f */
    inline extern const double SMALL_NUMBER         {1.e-8f};
    inline extern const double KINDA_SMALL_NUMBER   {1.e-8f};
    inline extern const double BIG_NUMBER           {3.4e+38f};
    inline extern const double HALF_CIRCLE          {180.00};
    inline extern const double FULL_CIRCLE          {360.00};
    inline extern const double GOLDEN_RATIO         {1.6180339887498948482045868343656381f}; /* Also known as divine proportion, golden mean, or golden section - related to the Fibonacci Sequence = (1 + sqrt(5)) / 2 */
    inline extern const double FLOAT_NON_FRACTIONAL {8388608.f}; /* All single-precision floating point numbers greater than or equal to this have no fractional value. */
}
    

class Math
{
public:
    /** Seeds global random number functions Rand() and FRand() */
    static void RandInit() { srand( time(0) ); }
    
    /** Returns a random integer between 0 and RAND_MAX, inclusive */
    static int Rand() { return rand(); }
    
    /** Returns a random float between 0 and 1, inclusive. */
    static float Rand01() 
    {
        // FP32 mantissa can only represent 24 bits before losing precision
        constexpr int RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
        return (Rand() & RandMax) / static_cast<float>(RandMax);
    }
    
    template <typename T, typename U>
    static auto RandomRange(T min, U max) {
        static std::random_device lRand;
        static std::mt19937 lGen(lRand());

        using lReturnType = std::common_type_t<T, U>; // Ensure compatible return type

        if constexpr (std::is_integral_v<lReturnType>) {
            std::uniform_int_distribution<lReturnType> lUniIntRange(min, max);
            return lUniIntRange(lGen);
        } else {
            std::uniform_real_distribution<lReturnType> lUniFloatRange(min, max);
            return lUniFloatRange(lGen);
        }
    }

    /** Return a uniformly distributed random unit length vector = point on the unit sphere surface. */
    [[nodiscard]] static Vector2D<float> VectorFRand()
    {
        FVector2D lResult;
        double lLength = StaticMath::BIG_NUMBER;

        do
        {
            // Check random vectors in the unit sphere so result is statistically uniform.
            lResult.x = Rand01() * 2.f - 1.f;
            lResult.y = Rand01() * 2.f - 1.f;
            lLength = lResult.SqLength();
        }
        while(lLength > 1.0f || lLength < StaticMath::KINDA_SMALL_NUMBER);

        return lResult * (1.0f / std::sqrt(lLength));
    }

    template<class T>
    [[nodiscard]] static constexpr auto RadiansToDegrees(T const& RadVal) -> decltype(RadVal * (StaticMath::HALF_CIRCLE / StaticMath::PI))
    {
        return RadVal * (StaticMath::HALF_CIRCLE / StaticMath::PI);
    }

    template<class T>
    [[nodiscard]] static constexpr auto DegreesToRadians(T const& DegVal) -> decltype(DegVal * (StaticMath::PI / StaticMath::HALF_CIRCLE))
    {
        return DegVal * (StaticMath::PI / StaticMath::HALF_CIRCLE);
    }
};
