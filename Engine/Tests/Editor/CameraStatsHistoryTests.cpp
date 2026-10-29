// Suite: what the editor's Camera Stats panel graphs (Editor/Camera/CameraStatsHistory.hpp).
#include <doctest.h>

#include "Editor/Camera/CameraStatsHistory.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    /** One pixel a world unit in a view this tall. */
    constexpr float HEIGHT_PX = 600.f;

    /** A frame at 60 Hz, the view and the target at x. */
    CameraTraceSample Frame(const float InViewX, const float InTargetX, const Uint32 InSteps = 1)
    {
        CameraTraceSample lSample;
        lSample.FrameDelta     = 1.0 / 60.0;
        lSample.Steps          = InSteps;
        lSample.Alpha          = 0.25;
        lSample.Camera         = "Cam";
        lSample.ViewPosition   = { InViewX, 0.f };
        lSample.ViewSize       = HEIGHT_PX * 0.5f;
        lSample.bHasTarget     = true;
        lSample.TargetPosition = { InTargetX, 40.f };
        return lSample;
    }
}

TEST_CASE("CameraStatsHistory: a locked target graphs no drift; a slide shows on its frame")
{
    CameraStatsHistory lHistory;
    CHECK_FALSE(lHistory.HasFrames());

    lHistory.Push(Frame(0.f, 0.f), HEIGHT_PX);
    lHistory.Push(Frame(30.f, 30.f, 2), HEIGHT_PX);
    lHistory.Push(Frame(60.f, 65.f, 0), HEIGHT_PX);

    REQUIRE(lHistory.Count() == 3);
    CHECK(lHistory.GetDrift()[0] == 0.f);
    CHECK(lHistory.GetDrift()[1] == doctest::Approx(0.f));
    CHECK(lHistory.GetDrift()[2] == doctest::Approx(5.f));
    CHECK(CameraStatsHistory::Largest(lHistory.GetDrift()) == doctest::Approx(5.f));

    CHECK(lHistory.GetFrameMs()[1] == doctest::Approx(1000.f / 60.f));
    CHECK(lHistory.GetSteps()[1] == 2.f);
    CHECK(lHistory.GetSteps()[2] == 0.f);
    CHECK(lHistory.GetAlpha()[2] == doctest::Approx(0.25f));
    CHECK(lHistory.GetLast().TargetPosition.x == 65.f);
}

TEST_CASE("CameraStatsHistory: keeps the last CAPACITY frames, oldest first; Clear starts over")
{
    CameraStatsHistory lHistory;
    for (Uint64 lFrame = 0; lFrame < CameraStatsHistory::CAPACITY + 10; ++lFrame)
    {
        lHistory.Push(Frame(0.f, 0.f, static_cast<Uint32>(lFrame % 3)), HEIGHT_PX);
    }

    REQUIRE(lHistory.Count() == CameraStatsHistory::CAPACITY);
    CHECK(lHistory.GetSteps().front() == static_cast<float>(10 % 3));
    CHECK(lHistory.GetSteps().back() == static_cast<float>((CameraStatsHistory::CAPACITY + 9) % 3));

    lHistory.Clear();
    CHECK(lHistory.Count() == 0);
    CHECK_FALSE(lHistory.HasFrames());

    // The first frame after a clear has no frame before it: no drift, wherever its target is.
    lHistory.Push(Frame(0.f, 500.f), HEIGHT_PX);
    CHECK(lHistory.GetDrift()[0] == 0.f);
}
