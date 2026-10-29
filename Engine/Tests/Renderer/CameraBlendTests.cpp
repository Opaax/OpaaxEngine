// Suite: blending two camera views (Renderer/Camera/CameraBlend.h) — the curves a blend eases along,
// and the view between two cameras: exact at its ends, the zoom in log space, the roll the short way.
#include <doctest.h>

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxEnumJson.h"
#include "Renderer/Camera/CameraBlend.h"

using namespace Opaax;

TEST_CASE("CameraBlendWeight: every curve goes from 0 to 1 without turning back; a cut is at 1 at once")
{
    for (const ECameraBlend lCurve : { ECameraBlend::Linear, ECameraBlend::EaseIn, ECameraBlend::EaseOut,
                                       ECameraBlend::EaseInOut })
    {
        CAPTURE(ToString(lCurve));
        CHECK(CameraBlendWeight(lCurve, 0.f) == 0.f);
        CHECK(CameraBlendWeight(lCurve, 1.f) == 1.f);

        // Clamped outside the blend.
        CHECK(CameraBlendWeight(lCurve, -0.5f) == 0.f);
        CHECK(CameraBlendWeight(lCurve, 1.5f) == 1.f);

        float lLast = 0.f;
        for (Int32 lStep = 1; lStep <= 100; ++lStep)
        {
            const float lWeight = CameraBlendWeight(lCurve, static_cast<float>(lStep) / 100.f);
            CHECK(lWeight >= lLast);
            lLast = lWeight;
        }
    }

    CHECK(CameraBlendWeight(ECameraBlend::Cut, 0.f) == 1.f);
}

TEST_CASE("CameraBlendWeight: the eases are slow where they say, and mirror each other")
{
    CHECK(CameraBlendWeight(ECameraBlend::Linear, 0.25f) == doctest::Approx(0.25f));
    CHECK(CameraBlendWeight(ECameraBlend::EaseIn, 0.25f) < 0.25f);
    CHECK(CameraBlendWeight(ECameraBlend::EaseOut, 0.25f) > 0.25f);
    CHECK(CameraBlendWeight(ECameraBlend::EaseInOut, 0.5f) == doctest::Approx(0.5f));
    CHECK(CameraBlendWeight(ECameraBlend::EaseInOut, 0.1f) < 0.1f);
    CHECK(CameraBlendWeight(ECameraBlend::EaseInOut, 0.9f) > 0.9f);

    for (const float lTime : { 0.1f, 0.3f, 0.7f })
    {
        CHECK(CameraBlendWeight(ECameraBlend::EaseIn, lTime)
              == doctest::Approx(1.f - CameraBlendWeight(ECameraBlend::EaseOut, 1.f - lTime)));
    }
}

TEST_CASE("BlendCameraViews: each end is its view exactly; the position halfway is the middle")
{
    const CameraView lFrom{ { 10.f, -20.f }, 100.f, 15.f };
    const CameraView lTo{ { 210.f, 80.f }, 400.f, 45.f };

    const CameraView lStart = BlendCameraViews(lFrom, lTo, 0.f);
    CHECK(lStart.Position.x == lFrom.Position.x);
    CHECK(lStart.OrthoSize == lFrom.OrthoSize);
    CHECK(lStart.Rotation == lFrom.Rotation);

    const CameraView lEnd = BlendCameraViews(lFrom, lTo, 1.f);
    CHECK(lEnd.Position.y == lTo.Position.y);
    CHECK(lEnd.OrthoSize == lTo.OrthoSize);
    CHECK(lEnd.Rotation == lTo.Rotation);

    const CameraView lHalfway = BlendCameraViews(lFrom, lTo, 0.5f);
    CHECK(lHalfway.Position.x == doctest::Approx(110.f));
    CHECK(lHalfway.Position.y == doctest::Approx(30.f));
    CHECK(lHalfway.Rotation == doctest::Approx(30.f));
}

TEST_CASE("BlendCameraViews: the size blends in log space, so each step of the blend zooms by the same ratio")
{
    const CameraView lFrom{ { 0.f, 0.f }, 100.f };
    const CameraView lTo{ { 0.f, 0.f }, 400.f };

    // The geometric mean halfway, and the same ratio over every quarter.
    CHECK(BlendCameraViews(lFrom, lTo, 0.5f).OrthoSize == doctest::Approx(200.f));

    const float lQuarter = BlendCameraViews(lFrom, lTo, 0.25f).OrthoSize;
    const float lThree   = BlendCameraViews(lFrom, lTo, 0.75f).OrthoSize;
    CHECK(lQuarter / 100.f == doctest::Approx(200.f / lQuarter));
    CHECK(lThree / 200.f == doctest::Approx(400.f / lThree));

    // A size that is not above zero blends linearly (the guard deals with it).
    CHECK(BlendCameraViews(CameraView{ { 0.f, 0.f }, 0.f }, lTo, 0.5f).OrthoSize == doctest::Approx(200.f));
}

TEST_CASE("BlendCameraViews: the roll turns the short way round")
{
    const CameraView lFrom{ { 0.f, 0.f }, 300.f, 170.f };
    const CameraView lTo{ { 0.f, 0.f }, 300.f, -170.f };

    // Through 180, not back through 0.
    CHECK(BlendCameraViews(lFrom, lTo, 0.5f).Rotation == doctest::Approx(180.f));
    CHECK(BlendCameraViews(lFrom, lTo, 0.25f).Rotation == doctest::Approx(175.f));

    const CameraView lWound{ { 0.f, 0.f }, 300.f, 350.f };
    CHECK(BlendCameraViews(lWound, CameraView{ { 0.f, 0.f }, 300.f, 10.f }, 0.5f).Rotation == doctest::Approx(360.f));
}

TEST_CASE("ECameraBlend: saved by its label")
{
    CHECK(nlohmann::json(ECameraBlend::EaseOut) == "EaseOut");
    CHECK(nlohmann::json("Linear").get<ECameraBlend>() == ECameraBlend::Linear);
    CHECK_THROWS(nlohmann::json("Wobbly").get<ECameraBlend>());
}
