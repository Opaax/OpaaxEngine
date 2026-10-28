// Suite: FrameClock — the delta of each frame, the cap on a long frame, the fixed steps owed,
// Restart (the time spent opening a level is not simulated), and a paused world held still.
// The fixed step is 1/64 s here so the arithmetic is exact.
#include <doctest.h>

#include "Engine/FrameClock.h"

using namespace Opaax;

namespace
{
    constexpr double FIXED = 1.0 / 64.0;

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
