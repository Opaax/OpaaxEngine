// Suite: FrameClock — the delta of each frame, the cap on a long frame, the fixed steps owed,
// Restart (the time spent opening a level is not simulated), a paused world held still, and deltas
// snapped to a screen's refresh: a noisy timer gives steady frames, a stated rate is measured, the
// game's time does not drift, and a frame at a step's length takes exactly one step.
// The fixed step is 1/64 s here so the arithmetic is exact.
#include <doctest.h>

#include "Core/Maths/Maths.h"
#include "Engine/FrameClock.h"

using namespace Opaax;

namespace
{
    constexpr double FIXED   = 1.0 / 64.0;
    constexpr double REFRESH = 1.0 / 128.0;

    /** How far frame InIndex's timestamp falls from its refresh: up to a quarter of one, either way. */
    double Noise(const int InIndex)
    {
        const double lOffsets[] = { 0.0, 0.25, -0.2, 0.1, -0.25, 0.05, 0.2, -0.1 };
        return lOffsets[InIndex % 8] * REFRESH;
    }

    /** Takes every fixed step the frame owes. */
    int TakeAll(FrameClock& InClock)
    {
        int lSteps = 0;
        while (InClock.TakeFixedStep())
        {
            ++lSteps;
        }
        return lSteps;
    }

    /** RHYTHM_FRAMES frames of a refresh each from InNow, so the clock snaps from the next one. */
    double KeepRhythm(FrameClock& InClock, double InNow)
    {
        for (Uint32 lFrame = 0; lFrame < FrameClock::RHYTHM_FRAMES; ++lFrame)
        {
            InNow += REFRESH;
            InClock.BeginFrame(InNow);
            TakeAll(InClock);
        }
        return InNow;
    }
}

TEST_CASE("FrameClock: the first frame has a zero delta, the next ones the time since the last")
{
    FrameClock lClock(FIXED);

    CHECK(lClock.BeginFrame(100.0) == 0.0);
    CHECK(TakeAll(lClock) == 0);

    CHECK(lClock.BeginFrame(100.0 + FIXED * 3.5) == FIXED * 3.5);
    CHECK(lClock.GetDelta() == FIXED * 3.5);
    CHECK(TakeAll(lClock) == 3);
}

TEST_CASE("FrameClock: the part of a step a frame did not cover carries to the next frame")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);

    lClock.BeginFrame(FIXED * 0.75);
    CHECK(TakeAll(lClock) == 0);
    CHECK(lClock.GetFixedAlpha() == 0.75);

    lClock.BeginFrame(FIXED * 1.5);
    CHECK(TakeAll(lClock) == 1);
    CHECK(lClock.GetFixedAlpha() == 0.5);
}

TEST_CASE("FrameClock: a frame longer than MAX_FRAME_DELTA counts as MAX_FRAME_DELTA")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);

    CHECK(lClock.BeginFrame(3.0) == FrameClock::MAX_FRAME_DELTA);
    CHECK(TakeAll(lClock) == 16);   // 0.25 s at 64 steps a second
}

TEST_CASE("FrameClock: Restart drops the time since the last frame and the steps owed")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);
    lClock.BeginFrame(FIXED * 0.5);

    // Opening a level took two seconds: the next frame counts from the end of it.
    lClock.Restart(2.5);
    CHECK(lClock.GetFixedAlpha() == 0.0);

    CHECK(lClock.BeginFrame(2.5 + FIXED * 1.5) == FIXED * 1.5);
    CHECK(TakeAll(lClock) == 1);
}

TEST_CASE("FrameClock: a clock going backwards gives a zero delta")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(10.0);

    CHECK(lClock.BeginFrame(9.0) == 0.0);
    CHECK(lClock.BeginFrame(9.0 + FIXED) == FIXED);
}

TEST_CASE("FrameClock: a new fixed delta keeps the time owed, in steps of the new length")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);

    lClock.BeginFrame(FIXED * 0.5);
    CHECK(TakeAll(lClock) == 0);

    // Half a long step owed is two quarter steps.
    lClock.SetFixedDelta(FIXED / 4.0);
    CHECK(lClock.GetFixedDelta() == FIXED / 4.0);
    CHECK(TakeAll(lClock) == 2);
    CHECK(lClock.GetFixedAlpha() == 0.0);

    lClock.BeginFrame(FIXED * 0.875);
    CHECK(TakeAll(lClock) == 1);
    CHECK(lClock.GetFixedAlpha() == 0.5);

    // Nothing above zero: kept.
    lClock.SetFixedDelta(0.0);
    lClock.SetFixedDelta(-1.0);
    CHECK(lClock.GetFixedDelta() == FIXED / 4.0);
}

TEST_CASE("FrameClock: a world that does not tick is drawn at its last alpha, and goes on from it")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);
    lClock.SetWorldTicks(true);

    lClock.BeginFrame(FIXED * 0.25);
    lClock.SetWorldTicks(true);
    CHECK(TakeAll(lClock) == 0);
    CHECK(lClock.GetDisplayAlpha() == 0.25);

    // Paused through frames that cross steps (the engine still takes them): drawn at 0.25 all along.
    double lNow = FIXED * 0.25;
    for (int lFrame = 0; lFrame < 10; ++lFrame)
    {
        lNow += FIXED * 0.375;
        lClock.BeginFrame(lNow);
        lClock.SetWorldTicks(false);
        TakeAll(lClock);
        CHECK(lClock.GetDisplayAlpha() == 0.25);
    }

    // Ticking again: half a step on from the picture, then a step crossed.
    lNow += FIXED * 0.5;
    lClock.BeginFrame(lNow);
    lClock.SetWorldTicks(true);
    CHECK(TakeAll(lClock) == 0);
    CHECK(lClock.GetDisplayAlpha() == 0.75);

    lNow += FIXED * 0.5;
    lClock.BeginFrame(lNow);
    lClock.SetWorldTicks(true);
    CHECK(TakeAll(lClock) == 1);
    CHECK(lClock.GetDisplayAlpha() == 0.25);
}

TEST_CASE("FrameClock: a paused world stepped one frame moves by that frame, then holds again")
{
    FrameClock lClock(FIXED);
    lClock.BeginFrame(0.0);
    lClock.SetWorldTicks(true);
    lClock.BeginFrame(FIXED * 0.5);
    lClock.SetWorldTicks(true);
    TakeAll(lClock);

    lClock.BeginFrame(FIXED * 1.25);
    lClock.SetWorldTicks(false);
    TakeAll(lClock);
    CHECK(lClock.GetDisplayAlpha() == 0.5);

    // The step: three quarters of a step on, one step crossed.
    lClock.BeginFrame(FIXED * 2.0);
    lClock.SetWorldTicks(true);
    CHECK(TakeAll(lClock) == 1);
    CHECK(lClock.GetDisplayAlpha() == 0.25);

    lClock.BeginFrame(FIXED * 3.0);
    lClock.SetWorldTicks(false);
    TakeAll(lClock);
    CHECK(lClock.GetDisplayAlpha() == 0.25);
}

TEST_CASE("FrameClock: a steady screen read by a noisy timer gives whole refreshes, and the time adds up")
{
    FrameClock lClock(FIXED);
    lClock.SetRefreshInterval(REFRESH, /*bInStated*/ false);
    lClock.BeginFrame(0.0);

    double lGiven     = 0.0;
    int    lLastSteps = -1;
    for (int lIndex = 1; lIndex <= 1000; ++lIndex)
    {
        const double lDelta = lClock.BeginFrame(lIndex * REFRESH + Noise(lIndex));
        const int    lSteps = TakeAll(lClock);
        lGiven += lDelta;

        // Snapped once RHYTHM_FRAMES kept to the refresh; two refreshes a step: one step every other frame.
        if (lIndex > static_cast<int>(FrameClock::RHYTHM_FRAMES))
        {
            CHECK(lDelta == REFRESH);
            CHECK(lSteps + lLastSteps == 1);
        }
        lLastSteps = lSteps;
    }

    CHECK(Maths::Abs(lGiven - (1000 * REFRESH + Noise(1000))) <= 0.25 * REFRESH);
}

TEST_CASE("FrameClock: a missed refresh is two; a stall, or a screen that does not wait, is as measured")
{
    FrameClock lClock(FIXED);
    lClock.SetRefreshInterval(REFRESH, false);
    lClock.BeginFrame(0.0);
    double lNow = KeepRhythm(lClock, 0.0);

    // A frame shown a refresh late; then one half-way between refreshes with what was carried.
    CHECK(lClock.BeginFrame(lNow + REFRESH * 2.1) == REFRESH * 2.0);
    CHECK(lClock.BeginFrame(lNow + REFRESH * 3.5) == doctest::Approx(REFRESH * 1.5));
    CHECK(lClock.BeginFrame(lNow + REFRESH * 4.5) == REFRESH);

    // Frames far shorter than a refresh: the screen does not wait (vsync not holding), as measured.
    CHECK(lClock.BeginFrame(lNow + REFRESH * 4.6) == doctest::Approx(REFRESH * 0.1));
    CHECK(lClock.BeginFrame(lNow + REFRESH * 4.7) == doctest::Approx(REFRESH * 0.1));

    // No refresh: every delta as measured.
    lClock.SetRefreshInterval(0.0, false);
    CHECK(lClock.BeginFrame(lNow + REFRESH * 6.0) == doctest::Approx(REFRESH * 1.3));
}

TEST_CASE("FrameClock: frames off the refresh in a row stop the snapping; frames back on it start it again")
{
    FrameClock lClock(FIXED);
    lClock.SetRefreshInterval(REFRESH, false);
    lClock.BeginFrame(0.0);
    double lNow = KeepRhythm(lClock, 0.0);

    // A lone stall does not stop it.
    lNow += REFRESH * 7.5;
    CHECK(lClock.BeginFrame(lNow) == doctest::Approx(REFRESH * 7.5));
    lNow += REFRESH * 1.1;
    CHECK(lClock.BeginFrame(lNow) == REFRESH);

    // Vsync not holding: short frames as measured (the first with the tenth carried), and the snapping stops.
    lNow += REFRESH * 0.1;
    CHECK(lClock.BeginFrame(lNow) == doctest::Approx(REFRESH * 0.2));
    for (Uint32 lFrame = 1; lFrame < FrameClock::RHYTHM_FRAMES; ++lFrame)
    {
        lNow += REFRESH * 0.1;
        CHECK(lClock.BeginFrame(lNow) == doctest::Approx(REFRESH * 0.1));
    }

    // Back on the refresh: as measured until RHYTHM_FRAMES kept to it, then snapped.
    for (Uint32 lFrame = 1; lFrame < FrameClock::RHYTHM_FRAMES; ++lFrame)
    {
        lNow += REFRESH * 1.1;
        CHECK(lClock.BeginFrame(lNow) == doctest::Approx(REFRESH * 1.1));
    }
    lNow += REFRESH * 1.1;
    CHECK(lClock.BeginFrame(lNow) == REFRESH);
}

TEST_CASE("FrameClock: time given ahead of the timer is never given back as a negative delta")
{
    FrameClock lClock(FIXED);
    lClock.SetRefreshInterval(REFRESH, false);
    lClock.BeginFrame(0.0);
    double lNow = KeepRhythm(lClock, 0.0);

    // Snapped up to a refresh, so ahead of the timer by 0.4 of one; the next frame is a stall short of it.
    lNow += REFRESH * 0.6;
    CHECK(lClock.BeginFrame(lNow) == REFRESH);
    lNow += REFRESH * 0.05;
    CHECK(lClock.BeginFrame(lNow) == 0.0);
    CHECK(lClock.GetFixedAlpha() >= 0.0);
}

TEST_CASE("FrameClock: a stated rate is measured: a screen at 59.94 Hz said to be 60 gets its own frames")
{
    const double lStated = 1.0 / 60.0;
    const double lReal   = 1.001 / 60.0;

    FrameClock lClock(1.0 / 60.0);
    lClock.SetRefreshInterval(lStated, /*bInStated*/ true);
    lClock.BeginFrame(0.0);

    double lGiven = 0.0;
    double lLast  = 0.0;
    for (int lIndex = 1; lIndex <= 3000; ++lIndex)
    {
        lLast   = lClock.BeginFrame(lIndex * lReal + 0.1 * Noise(lIndex));
        lGiven += lLast;
        TakeAll(lClock);
    }

    // Within about a thousandth of a percent, where the stated rate is a tenth of a percent off.
    CHECK(Maths::Abs(lClock.GetRefreshInterval() - lReal) < 2.0e-7);
    CHECK(Maths::Abs(lLast - lReal) < 2.0e-7);
    CHECK(Maths::Abs(lGiven - 3000 * lReal) <= 0.5 * lReal);   // the game's time keeps to the screen's
}

TEST_CASE("FrameClock: frames as long as a step take exactly one step each, sums of them included")
{
    const double lStep = 1.0 / 60.0;
    FrameClock   lClock(lStep);
    lClock.BeginFrame(0.0);

    // The timestamps a 60 Hz pacing adds up to: each delta is a step give or take the last bit.
    int lOther = 0;
    for (int lIndex = 1; lIndex <= 5000; ++lIndex)
    {
        lClock.BeginFrame(lIndex * lStep);
        lOther += TakeAll(lClock) != 1 ? 1 : 0;
        CHECK(lClock.GetFixedAlpha() >= 0.0);
    }
    CHECK(lOther == 0);
}
