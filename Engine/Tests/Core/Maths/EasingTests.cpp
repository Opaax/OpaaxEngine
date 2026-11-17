// Suite: Easing. The curves every UI transition and tween eases along. The ends must be exact (a
// motion that stops at 0.999 never rests), and the overshooting curves must overshoot.
#include <doctest.h>

#include "Core/Maths/Easing.h"
#include "Core/Reflection/OpaaxEnumJson.h"

using namespace Opaax;

namespace
{
    constexpr EEase MIRRORED_PAIRS[][2] = {
        { EEase::QuadIn, EEase::QuadOut },
        { EEase::CubicIn, EEase::CubicOut },
        { EEase::SineIn, EEase::SineOut },
        { EEase::BackIn, EEase::BackOut }
    };
}

TEST_CASE("Easing: every curve is 0 at 0 and 1 at 1, and clamps outside")
{
    for (const EEase lEase : TEnumValues<EEase>::Values)
    {
        const char* lName = ToString(lEase);
        CAPTURE(lName);
        CHECK(Easing::Evaluate(lEase, 0.f) == 0.f);
        CHECK(Easing::Evaluate(lEase, 1.f) == 1.f);
        CHECK(Easing::Evaluate(lEase, -3.f) == 0.f);
        CHECK(Easing::Evaluate(lEase, 7.f) == 1.f);
    }
}

TEST_CASE("Easing: an In is its Out mirrored")
{
    for (const auto& lPair : MIRRORED_PAIRS)
    {
        const char* lName = ToString(lPair[0]);
        CAPTURE(lName);
        for (float lTime = 0.05f; lTime < 1.f; lTime += 0.1f)
        {
            CHECK(Easing::Evaluate(lPair[0], lTime)
                  == doctest::Approx(1.f - Easing::Evaluate(lPair[1], 1.f - lTime)).epsilon(1e-4));
        }
    }
}

TEST_CASE("Easing: InOut curves are halfway at half time, and the shapes go the right way")
{
    CHECK(Easing::Evaluate(EEase::Linear, 0.3f) == doctest::Approx(0.3f));

    CHECK(Easing::Evaluate(EEase::QuadInOut, 0.5f) == doctest::Approx(0.5f));
    CHECK(Easing::Evaluate(EEase::CubicInOut, 0.5f) == doctest::Approx(0.5f));
    CHECK(Easing::Evaluate(EEase::SineInOut, 0.5f) == doctest::Approx(0.5f));
    CHECK(Easing::Evaluate(EEase::BackInOut, 0.5f) == doctest::Approx(0.5f));

    // In starts slowly (below the line), Out starts fast (above it).
    CHECK(Easing::Evaluate(EEase::QuadIn, 0.25f) < 0.25f);
    CHECK(Easing::Evaluate(EEase::QuadOut, 0.25f) > 0.25f);
    CHECK(Easing::Evaluate(EEase::CubicIn, 0.25f) < Easing::Evaluate(EEase::QuadIn, 0.25f));
}

TEST_CASE("Easing: Back dips below 0, Elastic passes 1, Bounce stays in range")
{
    CHECK(Easing::Evaluate(EEase::BackIn, 0.2f) < 0.f);
    CHECK(Easing::Evaluate(EEase::BackOut, 0.8f) > 1.f);
    CHECK(Easing::Evaluate(EEase::BackInOut, 0.1f) < 0.f);
    CHECK(Easing::Evaluate(EEase::BackInOut, 0.9f) > 1.f);

    bool bPassedOne = false;
    for (float lTime = 0.f; lTime <= 1.f; lTime += 0.01f)
    {
        bPassedOne = bPassedOne || Easing::Evaluate(EEase::ElasticOut, lTime) > 1.f;

        const float lBounce = Easing::Evaluate(EEase::BounceOut, lTime);
        CHECK(lBounce >= 0.f);
        CHECK(lBounce <= 1.f + 1e-5f);
    }
    CHECK(bPassedOne);
}

TEST_CASE("Easing: an EEase is saved by its name")
{
    nlohmann::json lJson;
    to_json(lJson, EEase::BackOut);
    CHECK(lJson == "BackOut");

    EEase lRead = EEase::Linear;
    from_json(nlohmann::json("SineInOut"), lRead);
    CHECK(lRead == EEase::SineInOut);
}
