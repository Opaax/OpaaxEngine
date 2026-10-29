// Suite: Maths, the engine's maths functions. Where a std function exists, Maths gives its result,
// edge cases included (equal values, -0, NaN), so engine code can use Maths everywhere.
#include <doctest.h>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/Templates/TypeTraits.h"

using namespace Opaax;

namespace
{
    const float NAN_F = TNumericLimits<float>::quiet_NaN();

    /** True for -0 and negative values: 1 / -0 is -infinity. */
    bool IsNegative(const float InValue) { return 1.f / InValue < 0.f; }
}

TEST_CASE("Maths::Abs: integers, floats, and -0 becomes +0")
{
    CHECK(Maths::Abs(-3) == 3);
    CHECK(Maths::Abs(Int64(-9000000000)) == Int64(9000000000));
    CHECK(Maths::Abs(-2.5f) == 2.5f);
    CHECK(Maths::Abs(2.5) == 2.5);

    CHECK(Maths::Abs(-0.f) == 0.f);
    CHECK_FALSE(IsNegative(Maths::Abs(-0.f)));
    CHECK(Maths::IsNaN(Maths::Abs(NAN_F)));

    static_assert(Maths::Abs(-4) == 4);
}

TEST_CASE("Maths::Max / Min: the larger and the smaller, NaN as std")
{
    CHECK(Maths::Max(1, 2) == 2);
    CHECK(Maths::Min(1, 2) == 1);
    CHECK(Maths::Max(-1.5f, -2.5f) == -1.5f);

    // A NaN first argument comes back; a NaN second argument is ignored.
    CHECK(Maths::IsNaN(Maths::Max(NAN_F, 1.f)));
    CHECK(Maths::Max(1.f, NAN_F) == 1.f);
    CHECK(Maths::IsNaN(Maths::Min(NAN_F, 1.f)));
    CHECK(Maths::Min(1.f, NAN_F) == 1.f);

    // Several values: the first largest / smallest.
    CHECK(Maths::Max({ 3, 9, 4, 9 }) == 9);
    CHECK(Maths::Min({ 3.f, -1.f, 4.f }) == -1.f);
    CHECK(Maths::Max({ 7 }) == 7);

    static_assert(Maths::Max(2u, 5u) == 5u);
    static_assert(Maths::Min({ 4, 2, 8 }) == 2);
}

TEST_CASE("Maths::IsFinite: numbers are, infinities and NaN are not")
{
    CHECK(Maths::IsFinite(0.f));
    CHECK(Maths::IsFinite(-1.5e30f));
    CHECK(Maths::IsFinite(TNumericLimits<double>::max()));
    CHECK_FALSE(Maths::IsFinite(NAN_F));
    CHECK_FALSE(Maths::IsFinite(TNumericLimits<float>::infinity()));
    CHECK_FALSE(Maths::IsFinite(-TNumericLimits<double>::infinity()));
}

TEST_CASE("Maths::Clamp: inside, below, above, and NaN passes through")
{
    CHECK(Maths::Clamp(0.5f, 0.f, 1.f) == 0.5f);
    CHECK(Maths::Clamp(-3.f, 0.f, 1.f) == 0.f);
    CHECK(Maths::Clamp(7.f, 0.f, 1.f) == 1.f);
    CHECK(Maths::Clamp(5, 5, 5) == 5);
    CHECK(Maths::Clamp(Uint32(12), 2u, 10u) == 10u);
    CHECK(Maths::IsNaN(Maths::Clamp(NAN_F, 0.f, 1.f)));
}

TEST_CASE("Maths: Floor, Ceil, Trunc keep the type of their argument")
{
    static_assert(TIsSame<decltype(Maths::Floor(1.f)), float>);
    static_assert(TIsSame<decltype(Maths::Floor(1.0)), double>);
    static_assert(TIsSame<decltype(Maths::Ceil(1.f)), float>);
    static_assert(TIsSame<decltype(Maths::Trunc(1.0)), double>);

    CHECK(Maths::Floor(-1.5f) == -2.f);
    CHECK(Maths::Floor(1.5) == 1.0);
    CHECK(Maths::Ceil(-1.5f) == -1.f);
    CHECK(Maths::Ceil(1.000001) == 2.0);
    CHECK(Maths::Trunc(-1.7f) == -1.f);
    CHECK(Maths::Trunc(1.7) == 1.0);
}

TEST_CASE("Maths::RoundHalfFromZero: halves away from zero, unlike RoundToFloat")
{
    CHECK(Maths::RoundHalfFromZero(2.5) == 3.0);
    CHECK(Maths::RoundHalfFromZero(-2.5) == -3.0);
    CHECK(Maths::RoundHalfFromZero(2.4f) == 2.f);
    CHECK(Maths::RoundHalfFromZero(-2.6f) == -3.f);

    // RoundToFloat rounds halves up.
    CHECK(Maths::RoundToFloat(-2.5f) == -2.f);
}

TEST_CASE("Maths::Atan2 is exact on every quadrant; FastAtan2 stays close")
{
    CHECK(Maths::Atan2(1.f, 1.f) == doctest::Approx(FQUARTER_PI));
    CHECK(Maths::Atan2(1.0, -1.0) == doctest::Approx(3.0 * DQUARTER_PI));
    CHECK(Maths::Atan2(-1.f, -1.f) == doctest::Approx(-3.f * FQUARTER_PI));
    CHECK(Maths::Atan2(0.f, -1.f) == doctest::Approx(FPI));
    CHECK(Maths::Atan2(0.f, 1.f) == 0.f);

    for (Int32 lStep = 0; lStep < 64; ++lStep)
    {
        const float lAngle = -FPI + FTWO_PI * (static_cast<float>(lStep) + 0.5f) / 64.f;
        const float lY     = Maths::Sin(lAngle) * 3.f;
        const float lX     = Maths::Cos(lAngle) * 3.f;
        CHECK(Maths::FastAtan2(lY, lX) == doctest::Approx(Maths::Atan2(lY, lX)).epsilon(1.e-5));
    }
}

TEST_CASE("Maths: Exp2, Log10, Remainder, FMod")
{
    CHECK(Maths::Exp2(3.f) == doctest::Approx(8.f));
    CHECK(Maths::Exp2(-1.0) == doctest::Approx(0.5));
    CHECK(Maths::Log10(1000.0) == doctest::Approx(3.0));
    CHECK(Maths::Log10(0.01f) == doctest::Approx(-2.f));

    // The nearest multiple, ties to the even one.
    CHECK(Maths::Remainder(370.f, 360.f) == doctest::Approx(10.f));
    CHECK(Maths::Remainder(350.f, 360.f) == doctest::Approx(-10.f));
    CHECK(Maths::Remainder(180.0, 360.0) == doctest::Approx(180.0));
    CHECK(Maths::Remainder(540.0, 360.0) == doctest::Approx(-180.0));

    // FMod keeps the sign of X; a divisor near zero gives 0 instead of NaN.
    CHECK(Maths::FMod(5.5, 2.0) == doctest::Approx(1.5));
    CHECK(Maths::FMod(-5.5f, 2.f) == doctest::Approx(-1.5f));
    CHECK(Maths::FMod(3.f, 0.f) == 0.f);
}

TEST_CASE("Maths::Lerp: same-type and mixed-type forms")
{
    CHECK(Maths::Lerp(0.f, 10.f, 0.25f) == doctest::Approx(2.5f));
    CHECK(Maths::Lerp(2.0, 4.0, 0.5f) == doctest::Approx(3.0));
    CHECK(Maths::Lerp(0, 10.0, 0.5f) == doctest::Approx(5.0));
}

// =============================================================================
// Smoothing: the same at any frame rate
// =============================================================================
namespace
{
    /** Where a value starting at 0 is after one second of frames at InRate a second, each moving it by InStep. */
    template <typename TStep>
    float OneSecond(const int InRate, TStep&& InStep)
    {
        float lValue = 0.f;
        for (int lFrame = 0; lFrame < InRate; ++lFrame)
        {
            lValue = InStep(lValue, 1.f / static_cast<float>(InRate));
        }
        return lValue;
    }
}

TEST_CASE("Maths::ExpDecay: the same after a second at 30, 60 or 144 frames; Lerp once a frame is not")
{
    const auto lDecay = [](const float InValue, const float InDelta) { return Maths::ExpDecay(InValue, 100.f, 5.f, InDelta); };
    const float lExpected = 100.f * (1.f - Maths::Exp(-5.f));

    CHECK(OneSecond(30, lDecay) == doctest::Approx(lExpected).epsilon(1e-5));
    CHECK(OneSecond(60, lDecay) == doctest::Approx(lExpected).epsilon(1e-5));
    CHECK(OneSecond(144, lDecay) == doctest::Approx(lExpected).epsilon(1e-5));

    // The habit it replaces: a fixed factor a frame goes further on a faster screen.
    const auto lLerp = [](const float InValue, float) { return Maths::Lerp(InValue, 100.f, 0.01f); };
    CHECK(OneSecond(144, lLerp) > OneSecond(60, lLerp) + 25.f);
}

TEST_CASE("Maths::ExpDecay: one step of dt is two of dt / 2; never past the target; vectors")
{
    const float lOnce  = Maths::ExpDecay(10.f, -40.f, 3.f, 0.1f);
    const float lTwice = Maths::ExpDecay(Maths::ExpDecay(10.f, -40.f, 3.f, 0.05f), -40.f, 3.f, 0.05f);
    CHECK(lOnce == doctest::Approx(lTwice).epsilon(1e-6));

    // Any frame time, even a stall of seconds: toward the target, never past it.
    for (const float lDelta : { 0.f, 1.f / 240.f, 0.5f, 10.f })
    {
        CAPTURE(lDelta);
        const float lMoved = Maths::ExpDecay(10.f, -40.f, 3.f, lDelta);
        CHECK(lMoved <= 10.f);
        CHECK(lMoved >= -40.f);
    }
    CHECK(Maths::ExpDecay(10.f, -40.f, 0.f, 1.f) == 10.f);

    // ln(2) a second halves the gap every second.
    CHECK(Maths::ExpDecay(0.f, 100.f, 0.6931472f, 1.f) == doctest::Approx(50.f));

    const Vector2F lVector = Maths::ExpDecay(Vector2F{ 0.f, 10.f }, Vector2F{ 100.f, -10.f }, 0.6931472f, 1.f);
    CHECK(lVector.x == doctest::Approx(50.f));
    CHECK(lVector.y == doctest::Approx(0.f).epsilon(1e-5));
}

TEST_CASE("Maths::SmoothDamp: one step of dt is two of dt / 2, at rest or moving")
{
    for (const float lStartVelocity : { 0.f, -300.f, 120.f })
    {
        CAPTURE(lStartVelocity);

        float       lOnceVelocity = lStartVelocity;
        const float lOnce         = Maths::SmoothDamp(100.f, 0.f, lOnceVelocity, 0.3f, 1.f / 30.f);

        float       lTwiceVelocity = lStartVelocity;
        const float lHalf          = Maths::SmoothDamp(100.f, 0.f, lTwiceVelocity, 0.3f, 1.f / 60.f);
        const float lTwice         = Maths::SmoothDamp(lHalf, 0.f, lTwiceVelocity, 0.3f, 1.f / 60.f);

        CHECK(lOnce == doctest::Approx(lTwice).epsilon(1e-5));
        CHECK(lOnceVelocity == doctest::Approx(lTwiceVelocity).epsilon(1e-4));
    }

    // And a second of 60 frames lands where one of 144 does.
    float lVelocity60  = 0.f;
    float lVelocity144 = 0.f;
    const float l60  = OneSecond(60, [&lVelocity60](const float InValue, const float InDelta)
    {
        return Maths::SmoothDamp(InValue, 100.f, lVelocity60, 0.4f, InDelta);
    });
    const float l144 = OneSecond(144, [&lVelocity144](const float InValue, const float InDelta)
    {
        return Maths::SmoothDamp(InValue, 100.f, lVelocity144, 0.4f, InDelta);
    });
    CHECK(l60 == doctest::Approx(l144).epsilon(1e-4));
}

TEST_CASE("Maths::SmoothDamp: from rest it eases toward the target and settles, never past it")
{
    for (const float lDelta : { 1.f / 240.f, 1.f / 60.f, 0.1f, 0.5f, 2.f })
    {
        CAPTURE(lDelta);

        float lValue    = 0.f;
        float lVelocity = 0.f;
        float lBefore   = 0.f;
        for (float lTime = 0.f; lTime < 3.f; lTime += lDelta)
        {
            lValue = Maths::SmoothDamp(lValue, 100.f, lVelocity, 0.5f, lDelta);
            CHECK(lValue >= lBefore);
            CHECK(lValue <= 100.f);
            lBefore = lValue;
        }
        CHECK(lValue == doctest::Approx(100.f).epsilon(1e-3));
    }

    // Thrown at the target faster than the spring holds: it stops on it rather than past it.
    float       lVelocity = 5000.f;
    const float lValue    = Maths::SmoothDamp(0.f, 10.f, lVelocity, 0.5f, 0.1f);
    CHECK(lValue == 10.f);
    CHECK(lVelocity == 0.f);

    // Vectors: the same spring on each axis.
    Vector2F       lVectorVelocity{ 0.f, 0.f };
    const Vector2F lVector = Maths::SmoothDamp(Vector2F{ 0.f, 0.f }, Vector2F{ 100.f, -50.f }, lVectorVelocity, 0.5f, 0.1f);
    float          lScalarVelocity = 0.f;
    CHECK(lVector.x == doctest::Approx(Maths::SmoothDamp(0.f, 100.f, lScalarVelocity, 0.5f, 0.1f)));
    CHECK(lVector.y == doctest::Approx(-0.5f * lVector.x));
}

TEST_CASE("Maths::Noise1D: replays, stays in [-1, 1] and uses the range, 0 at every whole X")
{
    float lLowest  = 0.f;
    float lHighest = 0.f;
    double lSum    = 0.0;
    for (Int32 lIndex = 0; lIndex < 20000; ++lIndex)
    {
        const double lX     = lIndex * 0.0137 - 50.0;
        const float  lValue = Maths::Noise1D(7u, lX);
        CHECK(lValue == Maths::Noise1D(7u, lX));
        lLowest  = Maths::Min(lLowest, lValue);
        lHighest = Maths::Max(lHighest, lValue);
        lSum    += lValue;
    }
    CHECK(lLowest >= -1.f);
    CHECK(lHighest <= 1.f);
    CHECK(lLowest < -0.5f);
    CHECK(lHighest > 0.5f);
    CHECK(Maths::Abs(lSum / 20000.0) < 0.1);

    for (const double lWhole : { -3.0, 0.0, 1.0, 42.0, 100000.0 })
    {
        CHECK(Maths::Noise1D(7u, lWhole) == 0.f);
    }
}

TEST_CASE("Maths::Noise1D: smooth, across whole X too, and far from 0")
{
    // Its slope stays bounded: no step anywhere, including where one cell meets the next.
    constexpr double STEP = 1.0e-4;
    for (Int32 lIndex = 0; lIndex < 5000; ++lIndex)
    {
        const double lX     = lIndex * 0.0211 - 20.0;
        const float  lSlope = (Maths::Noise1D(3u, lX + STEP) - Maths::Noise1D(3u, lX)) / static_cast<float>(STEP);
        CHECK(Maths::Abs(lSlope) < 4.f);
    }
    for (const double lWhole : { -1.0, 2.0, 17.0 })
    {
        CHECK(Maths::Abs(Maths::Noise1D(3u, lWhole - 1.0e-6)) < 1.0e-5f);
        CHECK(Maths::Abs(Maths::Noise1D(3u, lWhole + 1.0e-6)) < 1.0e-5f);
    }

    // Hours of game time at 20 Hz: still a smooth wobble, not a staircase.
    const double lFar = 3600.0 * 5.0 * 20.0 + 0.25;
    CHECK(Maths::Abs(Maths::Noise1D(3u, lFar + STEP) - Maths::Noise1D(3u, lFar)) < 4.f * static_cast<float>(STEP));
    CHECK(Maths::Noise1D(3u, lFar) != Maths::Noise1D(3u, lFar + 0.25));
}

TEST_CASE("Maths::Noise1D: another seed is another wobble")
{
    double lProduct = 0.0;
    double lSquaresA = 0.0;
    double lSquaresB = 0.0;
    for (Int32 lIndex = 0; lIndex < 20000; ++lIndex)
    {
        const double lX = lIndex * 0.0137;
        const double lA = Maths::Noise1D(1u, lX);
        const double lB = Maths::Noise1D(2u, lX);
        lProduct  += lA * lB;
        lSquaresA += lA * lA;
        lSquaresB += lB * lB;
    }
    CHECK(Maths::Abs(lProduct / Maths::Sqrt(lSquaresA * lSquaresB)) < 0.2);
}
