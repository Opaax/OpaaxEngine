// Suite: CameraTrace — the measures the camera's smoothness is checked with: a target locked on
// screen, a steady pan, a pop for one frame, game time running back, a paused view that moves, a zoom
// turning back, a camera switch (a cut, not a move), and a trace placed against a reference of one
// path: at shared times, between steps, at any pacing. A view 300 units tall (half-extent) drawn
// 600 pixels tall: one world unit is one pixel. Steps of 1/60 s.
#include <doctest.h>

#include "Core/Maths/Maths.h"
#include "Renderer/Camera/CameraTrace.h"

using namespace Opaax;

namespace
{
    constexpr float  HEIGHT_PX = 600.f;
    constexpr float  SIZE      = 300.f;
    constexpr double STEP      = 1.0 / 60.0;

    /** A frame of camera "Cam" at InTime, its view at InView, its target at InTarget, drawn between steps. */
    CameraTraceSample Frame(const double InTime, const Vector2F InView, const Vector2F InTarget, const float InSize = SIZE)
    {
        const double lSteps = InTime / STEP;

        CameraTraceSample lSample;
        lSample.GameTime       = InTime;
        lSample.StepDelta      = STEP;
        lSample.Alpha          = lSteps - static_cast<double>(Maths::FloorToInt64(lSteps));
        lSample.Camera         = OpaaxString("Cam");
        lSample.ViewPosition   = InView;
        lSample.ViewSize       = InSize;
        lSample.bHasTarget     = true;
        lSample.TargetPosition = InTarget;
        return lSample;
    }

    /** Where the pan is at InTime: InSpeed units a second along x. */
    float PanAt(const double InTime, const float InSpeed = 120.f)
    {
        return static_cast<float>(InTime * InSpeed);
    }

    /** A pan at InSpeed units a second, drawn at InRate frames a second for InFrames, the target 40 above. */
    TDynArray<CameraTraceSample> Pan(const double InRate, const Uint32 InFrames, const float InSpeed = 120.f)
    {
        TDynArray<CameraTraceSample> lSamples;
        for (Uint32 lIndex = 0; lIndex < InFrames; ++lIndex)
        {
            const double lTime = lIndex / InRate;
            const float  lX    = PanAt(lTime, InSpeed);
            lSamples.push_back(Frame(lTime, { lX, 0.f }, { lX, 40.f }));
        }
        return lSamples;
    }
}

TEST_CASE("CameraTrace: a camera locked on its target panning steadily has no drift and no jitter")
{
    const CameraTraceSummary lSummary = CameraTrace::Summarize(Pan(120.0, 240), HEIGHT_PX);

    CHECK(lSummary.Frames == 240);
    CHECK(lSummary.TargetDrift == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(lSummary.MotionJitter == doctest::Approx(0.f).epsilon(1e-3));
    CHECK(lSummary.LargestMove == doctest::Approx(1.f).epsilon(1e-3));   // 120 units a second at 120 Hz
    CHECK(lSummary.TimeReversals == 0);
    CHECK(lSummary.StillMoves == 0);
    CHECK(lSummary.ZoomReversals == 0);
    CHECK(lSummary.CameraSwitches == 0);
}

TEST_CASE("CameraTrace: a target off its spot for one frame is a drift, on the frame it shows")
{
    TDynArray<CameraTraceSample> lSamples = Pan(60.0, 10);
    lSamples[6].TargetPosition.x += 3.f;

    const CameraTraceSummary lSummary = CameraTrace::Summarize(lSamples, HEIGHT_PX);
    CHECK(lSummary.TargetDrift == doctest::Approx(3.f));
    CHECK(lSummary.TargetDriftFrame == 6);

    // In pixels: drawn twice as tall, the same move spans twice the pixels.
    CHECK(CameraTrace::Summarize(lSamples, HEIGHT_PX * 2.f).TargetDrift == doctest::Approx(6.f));
}

TEST_CASE("CameraTrace: a frame shown late is a jitter of the pan")
{
    TDynArray<CameraTraceSample> lSamples = Pan(60.0, 10);
    lSamples[5].ViewPosition.x   += 0.5f;
    lSamples[5].TargetPosition.x += 0.5f;

    const CameraTraceSummary lSummary = CameraTrace::Summarize(lSamples, HEIGHT_PX);
    CHECK(lSummary.TargetDrift == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(lSummary.MotionJitter == doctest::Approx(1.f));   // +0.5 on frame 5, then -0.5 back
    CHECK(lSummary.MotionJitterFrame == 6);
}

TEST_CASE("CameraTrace: game time running back, and a view moving while time stands still")
{
    TDynArray<CameraTraceSample> lSamples = Pan(60.0, 6);
    lSamples[3].GameTime = lSamples[1].GameTime;   // drawn earlier than frame 2
    lSamples[5].GameTime = lSamples[4].GameTime;   // same time, but the view moved

    const CameraTraceSummary lSummary = CameraTrace::Summarize(lSamples, HEIGHT_PX);
    CHECK(lSummary.TimeReversals == 1);
    CHECK(lSummary.StillMoves == 1);

    // Standing still while the time stands still is fine.
    TDynArray<CameraTraceSample> lPaused = { Frame(1.0, { 5.f, 5.f }, { 5.f, 45.f }), Frame(1.0, { 5.f, 5.f }, { 5.f, 45.f }) };
    CHECK(CameraTrace::Summarize(lPaused, HEIGHT_PX).StillMoves == 0);
}

TEST_CASE("CameraTrace: a zoom turning back is counted each time")
{
    TDynArray<CameraTraceSample> lSamples;
    const float lSizes[] = { 300.f, 302.f, 304.f, 304.f, 303.f, 302.f, 305.f };
    for (Uint32 lIndex = 0; lIndex < 7; ++lIndex)
    {
        lSamples.push_back(Frame(lIndex / 60.0, { 0.f, 0.f }, { 0.f, 40.f }, lSizes[lIndex]));
    }

    CHECK(CameraTrace::Summarize(lSamples, HEIGHT_PX).ZoomReversals == 2);
}

TEST_CASE("CameraTrace: a camera switch is a cut, not a move or a drift")
{
    TDynArray<CameraTraceSample> lSamples = Pan(60.0, 8);
    for (Uint64 lIndex = 4; lIndex < lSamples.size(); ++lIndex)
    {
        lSamples[lIndex].Camera          = OpaaxString("Other");
        lSamples[lIndex].ViewPosition.y += 80.f;   // the target now 40 below
    }

    const CameraTraceSummary lSummary = CameraTrace::Summarize(lSamples, HEIGHT_PX);
    CHECK(lSummary.CameraSwitches == 1);
    CHECK(lSummary.TargetDrift == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(lSummary.LargestMove == doctest::Approx(2.f));   // the pan, not the cut
}

TEST_CASE("CameraTrace: one path drawn at 60 and 120 frames a second matches its reference")
{
    const TDynArray<CameraTraceSample> lReference = Pan(60.0, 60);
    TDynArray<CameraTraceSample>       lAt120     = Pan(120.0, 118);

    // Every frame placed: the even ones at the reference's times, the odd ones halfway through a step.
    const CameraTraceGap lSame = CameraTrace::Compare(lReference, lAt120);
    CHECK(lSame.Shared == 118);
    CHECK(lSame.Position == doctest::Approx(0.f).epsilon(1e-4));
    CHECK(lSame.Size == 0.f);

    // A frame off by 0.5 is found at its time, wherever it falls.
    lAt120[21].ViewPosition.y += 0.5f;
    const CameraTraceGap lOff = CameraTrace::Compare(lReference, lAt120);
    CHECK(lOff.Position == doctest::Approx(0.5f));
    CHECK(lOff.AtTime == doctest::Approx(21.0 / 120.0));

    // Frames of another camera are not placed.
    for (CameraTraceSample& lSample : lAt120)
    {
        lSample.Camera = OpaaxString("Other");
    }
    CHECK(CameraTrace::Compare(lReference, lAt120).Shared == 0);
}

TEST_CASE("CameraTrace: a jittery trace of the same path matches; one drawn at its step's end does not")
{
    const TDynArray<CameraTraceSample> lReference = Pan(60.0, 61);

    // 144 frames a second, each up to a quarter of a frame early or late.
    TDynArray<CameraTraceSample> lJittery;
    for (Uint32 lIndex = 1; lIndex < 140; ++lIndex)
    {
        const double lTime = (lIndex + 0.25 * ((lIndex % 3) - 1.0)) / 144.0;
        lJittery.push_back(Frame(lTime, { PanAt(lTime), 0.f }, { PanAt(lTime), 40.f }));
    }

    const CameraTraceGap lSame = CameraTrace::Compare(lReference, lJittery);
    CHECK(lSame.Shared == 139);
    CHECK(lSame.Position == doctest::Approx(0.f).epsilon(1e-3));

    // Framed where its step ends instead of between steps (a view not blended): up to a step behind.
    TDynArray<CameraTraceSample> lUnblended = lJittery;
    for (CameraTraceSample& lSample : lUnblended)
    {
        const double lEnd = lSample.GameTime + (1.0 - lSample.Alpha) * STEP;
        lSample.ViewPosition.x = PanAt(lEnd);
    }
    CHECK(CameraTrace::Compare(lReference, lUnblended).Position > 0.5f);
}

TEST_CASE("CameraTrace: the CSV has a header and a row a frame, the target in pixels from the centre")
{
    TDynArray<CameraTraceSample> lSamples = Pan(60.0, 3);
    lSamples[2].bHasTarget = false;

    const OpaaxString lCsv = CameraTrace::ToCsv(lSamples, HEIGHT_PX);

    Uint32 lLines = 0;
    for (const char lChar : lCsv.View())
    {
        lLines += lChar == '\n' ? 1 : 0;
    }
    CHECK(lLines == 4);
    CHECK(lCsv.View().StartsWith("frame,delta,steps,alpha,stepDelta,gameTime,camera,viewX,viewY,viewSize,targetX,targetY,"
                                 "targetPxX,targetPxY\n"));
    CHECK(lCsv.Contains(",Cam,"));
    CHECK(lCsv.Contains(",0,40\n"));     // 40 units above the centre: 40 pixels
    CHECK(lCsv.View().EndsWith(",,,,\n"));
}
