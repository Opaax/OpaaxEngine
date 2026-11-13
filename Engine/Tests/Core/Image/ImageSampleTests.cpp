// Suite: ImageSample — what a part of an RGBA image holds: its different colours, their mean,
// the least and most of each channel. Pure: no window needed.
#include <doctest.h>

#include "Core/Image/ImageSample.h"

using namespace Opaax;

namespace
{
    /** A 4 by 2 image: the left half white, the right half black, opaque. */
    TDynArray<Uint8> TwoColours()
    {
        TDynArray<Uint8> lPixels;
        for (Uint32 lY = 0; lY < 2; ++lY)
        {
            for (Uint32 lX = 0; lX < 4; ++lX)
            {
                const Uint8 lValue = lX < 2 ? 255 : 0;
                lPixels.push_back(lValue);
                lPixels.push_back(lValue);
                lPixels.push_back(lValue);
                lPixels.push_back(255);
            }
        }
        return lPixels;
    }
}

TEST_CASE("ImageSample: a two-colour image has 2 colours, and their mean is between them")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    REQUIRE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 0, 0, 4, 2 }, lResult, lError));

    CHECK(lResult.Colors == 2);
    CHECK(lResult.Mean[0] == doctest::Approx(127.5));
    CHECK(lResult.Mean[3] == doctest::Approx(255.0));
    CHECK(lResult.Min[0] == 0);
    CHECK(lResult.Max[0] == 255);
    CHECK(lResult.Min[3] == 255);
    CHECK(lResult.Area.Width == 4);
    CHECK(lResult.Area.Height == 2);
}

TEST_CASE("ImageSample: a flat part has 1 colour")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    REQUIRE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 2, 0, 2, 2 }, lResult, lError));

    CHECK(lResult.Colors == 1);
    CHECK(lResult.Mean[0] == doctest::Approx(0.0));
    CHECK(lResult.Max[1] == 0);
    CHECK(lResult.Area.X == 2);
}

TEST_CASE("ImageSample: rows are read from the top")
{
    // 1 by 2: red above blue.
    const Uint8 lPixels[] = { 255, 0, 0, 255,   0, 0, 255, 255 };
    ImageSample::Result lResult;
    OpaaxString         lError;

    REQUIRE(ImageSample::Measure(lPixels, 1, 2, ImageSample::Rect{ 0, 1, 1, 1 }, lResult, lError));

    CHECK(lResult.Colors == 1);
    CHECK(lResult.Max[0] == 0);
    CHECK(lResult.Min[2] == 255);
}

TEST_CASE("ImageSample: an area past the image is cut to it")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    REQUIRE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ -5, -5, 7, 100 }, lResult, lError));

    CHECK(lResult.Area.X == 0);
    CHECK(lResult.Area.Y == 0);
    CHECK(lResult.Area.Width == 2);
    CHECK(lResult.Area.Height == 2);
    CHECK(lResult.Colors == 1);
    CHECK(lResult.Min[0] == 255);
}

TEST_CASE("ImageSample: an area with nothing inside the image fails, and says the image's size")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    CHECK_FALSE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 4, 0, 3, 3 }, lResult, lError));
    CHECK(lError.Contains("4 by 2"));

    CHECK_FALSE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ -9, 0, 3, 3 }, lResult, lError));
}

TEST_CASE("ImageSample: an empty area fails with what to give")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    CHECK_FALSE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 0, 0, 0, 2 }, lResult, lError));
    CHECK(lError.Contains("width"));

    CHECK_FALSE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 0, 0, 2, -1 }, lResult, lError));
}

TEST_CASE("ImageSample: no image fails")
{
    ImageSample::Result lResult;
    OpaaxString         lError;

    CHECK_FALSE(ImageSample::Measure(nullptr, 4, 2, ImageSample::Rect{ 0, 0, 1, 1 }, lResult, lError));
    CHECK_FALSE(lError.IsEmpty());
}

TEST_CASE("ImageSample: a far corner with a huge size does not wrap")
{
    const TDynArray<Uint8> lPixels = TwoColours();
    ImageSample::Result    lResult;
    OpaaxString            lError;

    REQUIRE(ImageSample::Measure(lPixels.data(), 4, 2, ImageSample::Rect{ 3, 1, 2147483647, 2147483647 }, lResult, lError));
    CHECK(lResult.Area.Width == 1);
    CHECK(lResult.Area.Height == 1);
}
