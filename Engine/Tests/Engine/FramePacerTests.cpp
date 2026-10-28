// Suite: FramePacing and FramePacer — the frames a paced run is given: the text a test writes, a
// steady rate, a jitter that never drifts off the rate, stalls where they are asked, and a list
// played in a loop. The same pacing must give the same frames every run.
// Rates of 64 and 128 keep the arithmetic exact.
#include <doctest.h>

#include "Core/Maths/Maths.h"
#include "Engine/FramePacer.h"

using namespace Opaax;

namespace
{
    FramePacing Parsed(const char* InText)
    {
        FramePacing lPacing;
        OpaaxString lError;
        REQUIRE_MESSAGE(FramePacing::Parse(InText, lPacing, lError), lError.CStr());
        return lPacing;
    }

    /** The error Parse gives InText, which must not be a pacing. */
    OpaaxString ParseError(const char* InText)
    {
        FramePacing lPacing;
        OpaaxString lError;
        CHECK_FALSE(FramePacing::Parse(InText, lPacing, lError));
        CHECK_FALSE(lError.IsEmpty());
        return lError;
    }
}

// =============================================================================
// Parse
// =============================================================================
TEST_CASE("FramePacing: reads a rate, a jitter, a hitch, a seed and a list")
{
    const FramePacing lRate = Parsed("144");
    CHECK(lRate.IsActive());
    CHECK(lRate.Rate == 144.0);
    CHECK(lRate.Jitter == 0.0);

    const FramePacing lFull = Parsed(" 60, jitter=0.25 ,hitch=0.1@2,seed=3 ");
    CHECK(lFull.Rate == 60.0);
    CHECK(lFull.Jitter == 0.25);
    CHECK(lFull.HitchSeconds == doctest::Approx(0.1));
    CHECK(lFull.HitchEvery == 2.0);
    CHECK(lFull.Seed == 3);

    const FramePacing lList = Parsed("deltas=0.016/0.033");
    CHECK(lList.IsActive());
    REQUIRE(lList.Deltas.size() == 2);
    CHECK(lList.Deltas[1] == doctest::Approx(0.033));

    CHECK(Parsed("rate=30").Rate == 30.0);
    CHECK_FALSE(Parsed("off").IsActive());
    CHECK_FALSE(Parsed("").IsActive());
}

TEST_CASE("FramePacing: refuses what is not a pacing, saying why")
{
    CHECK(ParseError("fast").Contains("rate"));
    CHECK(ParseError("0").Contains("above 0"));
    CHECK(ParseError("144,jitter=0.9").Contains("0.45"));
    CHECK(ParseError("144,hitch=0.1").Contains("0.1@2"));
    CHECK(ParseError("144,speed=2").Contains("unknown 'speed'"));
    CHECK(ParseError("144,0.25").Contains("key=value"));
    CHECK(ParseError("jitter=0.25").Contains("needs a rate"));
    CHECK(ParseError("144,deltas=0.016").Contains("not both"));
    CHECK(ParseError("deltas=0.016/-1").Contains("above 0"));
    CHECK(ParseError("60,seed=-2").Contains("whole number"));
}

TEST_CASE("FramePacing: written back as it reads")
{
    for (const char* lText : { "144", "60,jitter=0.25,hitch=0.1@2,seed=3", "deltas=0.016/0.033", "off" })
    {
        CAPTURE(lText);
        const FramePacing lPacing = Parsed(lText);
        const FramePacing lAgain  = Parsed(lPacing.ToString().CStr());

        CHECK(lPacing.ToString() == lText);
        CHECK(lAgain.Rate == lPacing.Rate);
        CHECK(lAgain.Jitter == lPacing.Jitter);
        CHECK(lAgain.Seed == lPacing.Seed);
        CHECK(lAgain.Deltas.size() == lPacing.Deltas.size());
    }
}

// =============================================================================
// FramePacer
// =============================================================================
TEST_CASE("FramePacer: a steady rate gives one tick a frame; no pacing gives nothing")
{
    FramePacer lPacer(Parsed("128"));
    for (int lIndex = 0; lIndex < 300; ++lIndex)
    {
        CHECK(lPacer.Next() == 1.0 / 128.0);
    }

    FramePacer lOff;
    CHECK(lOff.Next() == 0.0);
}

TEST_CASE("FramePacer: a jitter moves each frame, never the rate; a seed replays")
{
    const double lTick = 1.0 / 144.0;
    const double lJitter = 0.25;

    FramePacer lPacer(Parsed("144,jitter=0.25,seed=7"));
    FramePacer lSame(Parsed("144,jitter=0.25,seed=7"));
    FramePacer lOther(Parsed("144,jitter=0.25,seed=8"));

    double lSum       = 0.0;
    double lSmallest  = 1.0;
    double lLargest   = 0.0;
    bool   bReplays   = true;
    bool   bDifferent = false;

    constexpr int FRAMES = 1000;
    for (int lIndex = 0; lIndex < FRAMES; ++lIndex)
    {
        const double lDelta = lPacer.Next();
        bReplays   = bReplays && lDelta == lSame.Next();
        bDifferent = bDifferent || lDelta != lOther.Next();

        lSum      += lDelta;
        lSmallest  = Maths::Min(lSmallest, lDelta);
        lLargest   = Maths::Max(lLargest, lDelta);
    }

    CHECK(bReplays);
    CHECK(bDifferent);

    // Each frame is off its tick by at most a quarter of a frame, so a delta is within half a frame of
    // a tick, and the frames so far end within a quarter of a frame of their ticks.
    CHECK(lSmallest >= lTick * (1.0 - 2.0 * lJitter));
    CHECK(lLargest <= lTick * (1.0 + 2.0 * lJitter));
    CHECK(lLargest - lSmallest > lTick * lJitter);
    CHECK(Maths::Abs(lSum - FRAMES * lTick) <= lTick * lJitter);
}

TEST_CASE("FramePacer: a hitch stalls one frame each time it comes round")
{
    const double lTick = 1.0 / 64.0;

    FramePacer lPacer(Parsed("64,hitch=0.25@1"));

    TDynArray<int> lStalled;
    for (int lIndex = 1; lIndex <= 64 * 3; ++lIndex)
    {
        const double lDelta = lPacer.Next();
        if (lDelta != lTick)
        {
            CHECK(lDelta == lTick + 0.25);
            lStalled.push_back(lIndex);
        }
    }

    // The frames that end each second of ticks.
    REQUIRE(lStalled.size() == 3);
    CHECK(lStalled[0] == 64);
    CHECK(lStalled[1] == 128);
    CHECK(lStalled[2] == 192);
}

TEST_CASE("FramePacer: a list of deltas plays in a loop")
{
    FramePacer lPacer(Parsed("deltas=0.25/0.5/0.125"));

    for (int lLoop = 0; lLoop < 3; ++lLoop)
    {
        CHECK(lPacer.Next() == 0.25);
        CHECK(lPacer.Next() == 0.5);
        CHECK(lPacer.Next() == 0.125);
    }
}
