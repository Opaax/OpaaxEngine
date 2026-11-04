// Suite: FrameLimiter — how Window.MaxFrameRate holds the frames. A cap that divides the screen's rate
// is the swap interval (each frame the same whole refreshes, no wait); any other cap, or vsync off, a
// steady beat that a late frame restarts instead of catching up. A screen that ignores the swap
// interval falls back to the beat. 240 Hz rows: the user's screen.
#include <doctest.h>

#include "Core/Maths/Maths.h"
#include "Engine/FrameLimiter.h"

using namespace Opaax;

namespace
{
    constexpr double HZ_240 = 1.0 / 240.0;
    constexpr double HZ_60  = 1.0 / 60.0;

    // -------------------------------------------------------------------------
    // A frame loop on a screen: each frame works, waits as the limiter says, and presents. With vsync
    // the present returns at the flip: the first refresh at or after it, and whole refreshes after the
    // last flip when the screen honours the swap interval.
    // -------------------------------------------------------------------------
    struct Loop
    {
        FrameLimiter Limiter;
        double       Refresh  = 0.0;    // 0: no vsync, the present returns at once
        bool         bHonours = true;   // the screen shows a frame for the swap interval's refreshes

        double Now      = 1.0;
        double LastFlip = -1.0;

        Loop(const Uint32 InCap, const double InRefresh, const bool bInHonours = true)
            : Refresh(InRefresh), bHonours(bInHonours)
        {
            Limiter.SetMaxFrameRate(InCap);
            Limiter.SetRefreshInterval(InRefresh);
        }

        /** One frame of InWork seconds; returns when its present started (after the wait). */
        double Frame(const double InWork)
        {
            Now += InWork;
            Now += Limiter.WaitBeforePresent(Now);
            const double lStart = Now;

            if (Refresh > 0.0)
            {
                const Uint32 lHeld   = bHonours ? Maths::Max(Limiter.GetHeldRefreshes(), 1u) : 1u;
                const double lAfter  = LastFlip < 0.0 ? lStart : Maths::Max(lStart, LastFlip + lHeld * Refresh);
                const double lFlip   = Maths::Ceil(lAfter / Refresh - 1.0e-9) * Refresh;
                LastFlip = lFlip;
                Now      = lFlip;
            }

            Limiter.OnPresented(Now);
            return lStart;
        }

        /** Frames of InWork until InSeconds pass; how many there were. */
        Uint32 FramesIn(const double InSeconds, const double InWork)
        {
            const double lEnd    = Now + InSeconds;
            Uint32       lFrames = 0;
            while (Now < lEnd)
            {
                Frame(InWork);
                ++lFrames;
            }
            return lFrames;
        }
    };
}

// =============================================================================
// No cap
// =============================================================================

TEST_CASE("FrameLimiter: no cap waits for nothing and holds no refreshes")
{
    FrameLimiter lLimiter;
    lLimiter.SetRefreshInterval(HZ_240);
    for (double lNow = 0.0; lNow < 1.0; lNow += 0.001)
    {
        CHECK(lLimiter.WaitBeforePresent(lNow) == 0.0);
        lLimiter.OnPresented(lNow);
    }
    CHECK(lLimiter.GetHeldRefreshes() == 0);
    CHECK(lLimiter.GetMaxFrameRate() == 0);
}

// =============================================================================
// The beat
// =============================================================================

TEST_CASE("FrameLimiter: without vsync, the beat lets one frame through each 1 / cap")
{
    for (const Uint32 lCap : { 30u, 60u, 144u, 240u })
    {
        CAPTURE(lCap);
        Loop lLoop(lCap, 0.0);
        const double lPeriod = 1.0 / lCap;

        double lLast = lLoop.Frame(0.002);
        for (int lFrame = 0; lFrame < 200; ++lFrame)
        {
            const double lStart = lLoop.Frame(0.002);
            CHECK(lStart - lLast == doctest::Approx(lPeriod).epsilon(1.0e-9));
            lLast = lStart;
        }
        CHECK(lLoop.Limiter.GetHeldRefreshes() == 0);
    }
}

TEST_CASE("FrameLimiter: the first frame waits for nothing; a late one neither, and the next counts from it")
{
    FrameLimiter lLimiter;
    lLimiter.SetMaxFrameRate(100);   // 10 ms

    CHECK(lLimiter.WaitBeforePresent(5.0) == 0.0);
    CHECK(lLimiter.WaitBeforePresent(5.004) == doctest::Approx(0.006));   // on the beat: 5.010

    // A 25 ms stall: shown at once, then a whole period after it, not a burst of frames to catch up.
    CHECK(lLimiter.WaitBeforePresent(5.035) == 0.0);
    CHECK(lLimiter.WaitBeforePresent(5.036) == doctest::Approx(0.009));   // 5.045
    CHECK(lLimiter.WaitBeforePresent(5.050) == doctest::Approx(0.005));   // 5.055

    // Restart: the next frame waits for nothing.
    lLimiter.Restart();
    CHECK(lLimiter.WaitBeforePresent(5.048) == 0.0);
}

TEST_CASE("FrameLimiter: frames slower than the cap are never held")
{
    Loop lLoop(60, 0.0);
    for (int lFrame = 0; lFrame < 100; ++lFrame)
    {
        const double lBefore = lLoop.Now;
        lLoop.Frame(0.020);   // 50 frames a second
        CHECK(lLoop.Now - lBefore == doctest::Approx(0.020));
    }
}

TEST_CASE("FrameLimiter: with vsync, a cap that does not divide the screen's rate is the beat, on whole refreshes")
{
    struct Row { Uint32 Cap; double Refresh; };
    for (const Row lRow : { Row{ 100, HZ_240 }, Row{ 144, HZ_240 }, Row{ 165, HZ_240 }, Row{ 50, HZ_60 },
                            Row{ 240, HZ_240 }, Row{ 300, HZ_240 } })
    {
        CAPTURE(lRow.Cap);
        CAPTURE(1.0 / lRow.Refresh);
        Loop lLoop(lRow.Cap, lRow.Refresh);
        CHECK(lLoop.Limiter.GetHeldRefreshes() == 0);

        // Never more than the cap, nor more than the screen shows.
        lLoop.FramesIn(1.0, 0.001);
        const double lExpected = Maths::Min(static_cast<double>(lRow.Cap), 1.0 / lRow.Refresh);
        CHECK(static_cast<double>(lLoop.FramesIn(4.0, 0.001)) / 4.0 == doctest::Approx(lExpected).epsilon(0.01));
    }
}

// =============================================================================
// Whole refreshes
// =============================================================================

TEST_CASE("FrameLimiter: with vsync, a cap that divides the screen's rate holds each frame for whole refreshes")
{
    struct Row { Uint32 Cap; double Hertz; Uint32 Held; };
    for (const Row lRow : { Row{ 120, 240.0, 2 }, Row{ 80, 240.0, 3 }, Row{ 60, 240.0, 4 }, Row{ 30, 240.0, 8 },
                            Row{ 72, 144.0, 2 }, Row{ 30, 60.0, 2 }, Row{ 30, 59.94, 2 }, Row{ 60, 239.96, 4 },
                            Row{ 144, 240.0, 0 }, Row{ 100, 240.0, 0 }, Row{ 60, 144.0, 0 } })
    {
        CAPTURE(lRow.Cap);
        CAPTURE(lRow.Hertz);
        FrameLimiter lLimiter;
        lLimiter.SetMaxFrameRate(lRow.Cap);
        lLimiter.SetRefreshInterval(1.0 / lRow.Hertz);
        CHECK(lLimiter.GetHeldRefreshes() == lRow.Held);
    }

    // Without vsync there are no refreshes to hold for.
    FrameLimiter lNoVSync;
    lNoVSync.SetMaxFrameRate(120);
    CHECK(lNoVSync.GetHeldRefreshes() == 0);
}

TEST_CASE("FrameLimiter: held by the swap interval, a frame never waits, and each lasts the same refreshes")
{
    Loop lLoop(120, HZ_240);
    REQUIRE(lLoop.Limiter.GetHeldRefreshes() == 2);

    lLoop.Frame(0.001);
    double lLastFlip = lLoop.LastFlip;
    for (int lFrame = 0; lFrame < 480; ++lFrame)
    {
        const double lBefore = lLoop.Now + 0.001;
        CHECK(lLoop.Frame(0.001) == doctest::Approx(lBefore));   // no wait before the present
        CHECK(lLoop.LastFlip - lLastFlip == doctest::Approx(2.0 * HZ_240));
        lLastFlip = lLoop.LastFlip;
    }
    CHECK_FALSE(lLoop.Limiter.IsRefreshUnheld());

    // A late frame (three refreshes) is not an early one.
    lLoop.Frame(0.009);
    lLoop.FramesIn(1.0, 0.001);
    CHECK_FALSE(lLoop.Limiter.IsRefreshUnheld());
    CHECK(lLoop.Limiter.GetHeldRefreshes() == 2);
}

TEST_CASE("FrameLimiter: a screen that shows every refresh whatever the swap interval falls back to the beat")
{
    Loop lLoop(120, HZ_240, /*bInHonours*/ false);
    REQUIRE(lLoop.Limiter.GetHeldRefreshes() == 2);

    for (Uint32 lFrame = 0; lFrame <= FrameLimiter::UNHELD_FRAMES; ++lFrame)
    {
        lLoop.Frame(0.001);
    }
    CHECK(lLoop.Limiter.IsRefreshUnheld());
    CHECK(lLoop.Limiter.GetHeldRefreshes() == 0);

    // The beat holds the cap: 120 frames a second, not 240.
    lLoop.FramesIn(0.5, 0.001);
    CHECK(static_cast<double>(lLoop.FramesIn(4.0, 0.001)) / 4.0 == doctest::Approx(120.0).epsilon(0.01));

    // A new cap, or vsync turned on again, tries the swap interval again.
    lLoop.Limiter.SetMaxFrameRate(80);
    CHECK(lLoop.Limiter.GetHeldRefreshes() == 3);

    for (Uint32 lFrame = 0; lFrame <= FrameLimiter::UNHELD_FRAMES; ++lFrame)
    {
        lLoop.Frame(0.001);
    }
    REQUIRE(lLoop.Limiter.IsRefreshUnheld());
    lLoop.Limiter.SetRefreshInterval(0.0);
    lLoop.Limiter.SetRefreshInterval(HZ_240);
    CHECK_FALSE(lLoop.Limiter.IsRefreshUnheld());
    CHECK(lLoop.Limiter.GetHeldRefreshes() == 3);
}

TEST_CASE("FrameLimiter: a few early presents (a driver's queue filling) keep the swap interval")
{
    FrameLimiter lLimiter;
    lLimiter.SetMaxFrameRate(120);
    lLimiter.SetRefreshInterval(HZ_240);

    // Three frames back to back, then on the screen's rhythm.
    double lNow = 2.0;
    for (int lFrame = 0; lFrame < 3; ++lFrame)
    {
        lNow += 0.001;
        lLimiter.OnPresented(lNow);
    }
    for (int lFrame = 0; lFrame < 100; ++lFrame)
    {
        lNow += 2.0 * HZ_240;
        lLimiter.OnPresented(lNow);
    }
    CHECK_FALSE(lLimiter.IsRefreshUnheld());
    CHECK(lLimiter.GetHeldRefreshes() == 2);
}
