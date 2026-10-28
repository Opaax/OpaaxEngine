// Suite: Maths, the engine's maths functions. Where a std function exists, Maths gives its result,
// edge cases included (equal values, -0, NaN), so engine code can use Maths everywhere.
#include <doctest.h>

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
