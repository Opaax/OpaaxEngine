// Suite: where a new floating asset window opens — centred when it is the only one, then cascaded so
// every title bar stays visible.
#include <doctest.h>

#include "Editor/Assets/AssetWindowPlacement.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    // A 1280x720 editor whose menu bar takes the top 20 pixels.
    const Vector2F k_WorkPos  = { 0.f, 20.f };
    const Vector2F k_WorkSize = { 1280.f, 700.f };
    const Vector2F k_Window   = { 480.f, 540.f };
}

TEST_CASE("AssetWindowPlacement: the first window opens in the middle of the work area")
{
    const Vector2F lPos = CenteredWindowPos(k_WorkPos, k_WorkSize, k_Window);
    CHECK(lPos.x == doctest::Approx(400.f));
    CHECK(lPos.y == doctest::Approx(100.f));   // 20 + (700 - 540) / 2

    // Larger than the work area: its corner, so the title bar is never above the menu bar.
    const Vector2F lHuge = CenteredWindowPos(k_WorkPos, k_WorkSize, Vector2F{ 2000.f, 900.f });
    CHECK(lHuge.x == doctest::Approx(0.f));
    CHECK(lHuge.y == doctest::Approx(20.f));
}

TEST_CASE("AssetWindowPlacement: each next window steps below and right of the newest")
{
    const Vector2F lFirst  = CenteredWindowPos(k_WorkPos, k_WorkSize, k_Window);
    const Vector2F lSecond = CascadedWindowPos(lFirst, 22.f, k_Window, k_WorkPos, k_WorkSize);
    CHECK(lSecond.x == doctest::Approx(422.f));
    CHECK(lSecond.y == doctest::Approx(122.f));

    // Never the same corner twice in a row: the newest window's title bar stays visible.
    const Vector2F lThird = CascadedWindowPos(lSecond, 22.f, k_Window, k_WorkPos, k_WorkSize);
    CHECK(lThird.x > lSecond.x);
    CHECK(lThird.y > lSecond.y);
}

TEST_CASE("AssetWindowPlacement: a step past the right or bottom edge starts again at the corner")
{
    // 180 + 22 + 540 = 742 > 720: the window would leave the work area at the bottom.
    const Vector2F lBottom = CascadedWindowPos(Vector2F{ 400.f, 180.f }, 22.f, k_Window, k_WorkPos, k_WorkSize);
    CHECK(lBottom.x == doctest::Approx(0.f));
    CHECK(lBottom.y == doctest::Approx(20.f));

    // 790 + 22 + 480 = 1292 > 1280: past the right edge.
    const Vector2F lRight = CascadedWindowPos(Vector2F{ 790.f, 30.f }, 22.f, k_Window, k_WorkPos, k_WorkSize);
    CHECK(lRight.x == doctest::Approx(0.f));
    CHECK(lRight.y == doctest::Approx(20.f));

    // Exactly touching both edges still fits.
    const Vector2F lEdge = CascadedWindowPos(Vector2F{ 778.f, 158.f }, 22.f, k_Window, k_WorkPos, k_WorkSize);
    CHECK(lEdge.x == doctest::Approx(800.f));
    CHECK(lEdge.y == doctest::Approx(180.f));
}
