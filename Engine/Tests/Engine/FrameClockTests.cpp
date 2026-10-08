// Suite: FrameClock — the delta of each frame, the cap on a long frame, the fixed steps owed, and
// Restart (the time spent opening a level is not simulated).
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
