// Suite: the camera view math (Renderer/CameraView.h).
//
// Two things worth pinning. The DEFAULT view is load-bearing — it is what a world nobody
// produced a view for renders with, so it must stay byte-for-byte the frame the engine drew
// before cameras existed. And ScreenToWorld is the one screen->world rule in the tree; the
// editor's zoom-at-cursor is built on it round-tripping exactly.
#include <doctest.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/CameraView.h"

using namespace Opaax;

namespace
{
    // glm has no operator== worth trusting on floats — compare element-wise with a tolerance.
    void CheckMatrixEqual(const Matrix44F& InA, const Matrix44F& InB)
    {
        for (int lCol = 0; lCol < 4; ++lCol)
        {
            for (int lRow = 0; lRow < 4; ++lRow)
            {
                CHECK(InA[lCol][lRow] == doctest::Approx(InB[lCol][lRow]));
            }
        }
    }
}

TEST_CASE("CameraView: the DEFAULT view is the pre-camera centred frame")
{
    // The exact expression RendererManager hard-coded before this system existed, at the
    // viewport's default size. If this ever drifts, every map authored so far reframes.
    const Matrix44F lLegacy = glm::ortho(-480.f, 480.f, -300.f, 300.f, -1.f, 1.f);

    CheckMatrixEqual(MakeViewProjection(CameraView{}, 960, 600), lLegacy);
}

TEST_CASE("CameraView: OrthoSize is the VERTICAL half-extent; width follows the aspect")
{
    const CameraView lView{ { 0.f, 0.f }, 100.f };

    // 2:1 target -> 200 units wide, 100 tall (half-extents). A point on the right edge maps to
    // NDC +1, and one at the top edge to NDC +1 — that is what "half-extent" has to mean.
    const Matrix44F lVP  = MakeViewProjection(lView, 400, 200);
    const Vector4F  lRight = lVP * Vector4F(200.f, 0.f, 0.f, 1.f);
    const Vector4F  lTop   = lVP * Vector4F(0.f, 100.f, 0.f, 1.f);

    CHECK(lRight.x == doctest::Approx(1.f));
    CHECK(lTop.y   == doctest::Approx(1.f));
}

TEST_CASE("CameraView: the view translates by -Position, so the world moves opposite")
{
    const CameraView lView{ { 50.f, -25.f }, 300.f };
    const Matrix44F  lVP = MakeViewProjection(lView, 960, 600);

    // Whatever the camera sits on lands dead centre.
    const Vector4F lCentre = lVP * Vector4F(50.f, -25.f, 0.f, 1.f);
    CHECK(lCentre.x == doctest::Approx(0.f));
    CHECK(lCentre.y == doctest::Approx(0.f));
}

TEST_CASE("CameraView: a zero dimension yields identity rather than a divide")
{
    CheckMatrixEqual(MakeViewProjection(CameraView{}, 0, 600), Matrix44F(1.f));
    CheckMatrixEqual(MakeViewProjection(CameraView{}, 960, 0), Matrix44F(1.f));
}

TEST_CASE("CameraView: the halves multiply back to the whole")
{
    // We split MakeView/MakeProjection out for ImGuizmo, which takes them separately. The split is
    // only safe while the product still IS MakeViewProjection — a camera off the origin at a
    // non-square aspect is where a swapped multiplication order or a dropped translation shows up.
    const CameraView lView{ { 137.f, -64.f }, 250.f };

    CheckMatrixEqual(MakeProjection(lView, 1280, 720) * MakeView(lView),
                     MakeViewProjection(lView, 1280, 720));
}

TEST_CASE("CameraView: a degenerate target is identity for the WHOLE, not just the projection")
{
    // MakeViewProjection cannot defer its zero-guard to MakeProjection: identity * view is the
    // VIEW, so a camera away from the origin would answer a translation instead of identity.
    const CameraView lOffOrigin{ { 500.f, 500.f }, 300.f };

    CheckMatrixEqual(MakeViewProjection(lOffOrigin, 0, 600), Matrix44F(1.f));
    CheckMatrixEqual(MakeProjection(lOffOrigin, 0, 600), Matrix44F(1.f));
}

TEST_CASE("MakeView: translates by -Position and does not scale")
{
    const Matrix44F lView = MakeView(CameraView{ { 30.f, -12.f }, 999.f });

    // OrthoSize is the projection's business — a view that read it would frame twice.
    const Vector4F lMoved = lView * Vector4F(30.f, -12.f, 0.f, 1.f);

    CHECK(lMoved.x == doctest::Approx(0.f));
    CHECK(lMoved.y == doctest::Approx(0.f));
    CHECK(lView[0][0] == doctest::Approx(1.f));
    CHECK(lView[1][1] == doctest::Approx(1.f));
}

TEST_CASE("ScreenToWorld: the viewport centre is the camera's position")
{
    const CameraView lView{ { 12.f, -34.f }, 300.f };
    const Vector2F   lViewport{ 960.f, 600.f };

    const Vector2F lWorld = ScreenToWorld(lView, lViewport, lViewport * 0.5f);

    CHECK(lWorld.x == doctest::Approx(12.f));
    CHECK(lWorld.y == doctest::Approx(-34.f));
}

TEST_CASE("ScreenToWorld: the corners are the view's extents, with Y flipped")
{
    const CameraView lView{ { 0.f, 0.f }, 300.f };
    const Vector2F   lViewport{ 960.f, 600.f };  // aspect 1.6 -> 480 x 300 half-extents

    // Screen top-left (0,0) is world (-halfW, +halfH): screen-Y grows DOWN, world-Y grows UP.
    const Vector2F lTopLeft = ScreenToWorld(lView, lViewport, { 0.f, 0.f });
    CHECK(lTopLeft.x == doctest::Approx(-480.f));
    CHECK(lTopLeft.y == doctest::Approx(300.f));

    const Vector2F lBottomRight = ScreenToWorld(lView, lViewport, lViewport);
    CHECK(lBottomRight.x == doctest::Approx(480.f));
    CHECK(lBottomRight.y == doctest::Approx(-300.f));
}

TEST_CASE("ScreenToWorld: zoom is anchored — the same pixel moves in world space when OrthoSize does")
{
    // The whole mechanism behind zoom-at-cursor: read the world point under a pixel, change the
    // size, read it again, translate by the difference. If these two agreed, the anchor would be
    // a no-op and the editor's zoom would drift off the cursor.
    const Vector2F lViewport{ 800.f, 400.f };
    const Vector2F lCursor{ 600.f, 100.f };

    const Vector2F lBefore = ScreenToWorld(CameraView{ { 0.f, 0.f }, 200.f }, lViewport, lCursor);
    const Vector2F lAfter  = ScreenToWorld(CameraView{ { 0.f, 0.f }, 100.f }, lViewport, lCursor);

    CHECK(lBefore.x == doctest::Approx(lAfter.x * 2.f));
    CHECK(lBefore.y == doctest::Approx(lAfter.y * 2.f));
    CHECK(lBefore.x != doctest::Approx(lAfter.x));
}

TEST_CASE("ScreenToWorld: a degenerate viewport answers the camera position, not a NaN")
{
    const CameraView lView{ { 7.f, 8.f }, 300.f };

    CHECK(ScreenToWorld(lView, { 0.f, 600.f }, { 10.f, 10.f }).x == doctest::Approx(7.f));
    CHECK(ScreenToWorld(lView, { 960.f, 0.f }, { 10.f, 10.f }).y == doctest::Approx(8.f));
}

TEST_CASE("WorldToScreen: the exact inverse of ScreenToWorld, corners and an off-centre camera alike")
{
    // The overlay a UI panel draws over its preview is only right if the two directions agree —
    // which is why the inverse lives beside the rule instead of being re-derived at the call site.
    const CameraView lView{ { 12.f, -34.f }, 300.f };
    const Vector2F   lViewport{ 960.f, 600.f };

    const Vector2F lPixels[] = { { 0.f, 0.f }, lViewport, { 123.f, 456.f }, lViewport * 0.5f };
    for (const Vector2F& lPx : lPixels)
    {
        const Vector2F lBack = WorldToScreen(lView, lViewport, ScreenToWorld(lView, lViewport, lPx));
        CHECK(lBack.x == doctest::Approx(lPx.x));
        CHECK(lBack.y == doctest::Approx(lPx.y));
    }

    // And the flip, stated directly: world +Y is UP the screen.
    const Vector2F lAbove = WorldToScreen(lView, lViewport, { 12.f, -34.f + 150.f });
    CHECK(lAbove.x == doctest::Approx(480.f));
    CHECK(lAbove.y == doctest::Approx(150.f));   // half the 300-unit half-height, from the centre

    CHECK(WorldToScreen(lView, { 0.f, 600.f }, { 1.f, 1.f }).x == doctest::Approx(0.f));   // degenerate
}

TEST_CASE("ZoomAround: the world point under the anchor stays under it, for any anchor and size")
{
    const CameraView lView{ { 123.f, -45.f }, 300.f };
    const Vector2F   lViewport{ 1280.f, 720.f };

    for (const Vector2F lAnchor : { Vector2F{ 640.f, 360.f }, Vector2F{ 0.f, 0.f }, Vector2F{ 1280.f, 720.f },
                                    Vector2F{ 1000.f, 90.f } })
    {
        for (const float lSize : { 12.f, 250.f, 300.f, 4000.f })
        {
            CAPTURE(lAnchor.x);
            CAPTURE(lAnchor.y);
            CAPTURE(lSize);

            const CameraView lZoomed = ZoomAround(lView, lSize, lViewport, lAnchor);
            const Vector2F   lBefore = ScreenToWorld(lView, lViewport, lAnchor);
            const Vector2F   lAfter  = ScreenToWorld(lZoomed, lViewport, lAnchor);

            CHECK(lZoomed.OrthoSize == lSize);
            CHECK(lAfter.x == doctest::Approx(lBefore.x).epsilon(1e-5));
            CHECK(lAfter.y == doctest::Approx(lBefore.y).epsilon(1e-5));
        }
    }
}

TEST_CASE("ZoomAround: at the centre only the size changes; off centre the view moves toward the anchor")
{
    const CameraView lView{ { 10.f, 20.f }, 300.f };
    const Vector2F   lViewport{ 800.f, 600.f };

    const CameraView lCentred = ZoomAround(lView, 150.f, lViewport, lViewport * 0.5f);
    CHECK(lCentred.Position.x == doctest::Approx(10.f));
    CHECK(lCentred.Position.y == doctest::Approx(20.f));

    // Zooming in at the top right corner: the view moves halfway there (to the right, up).
    const CameraView lCorner = ZoomAround(lView, 150.f, lViewport, { 800.f, 0.f });
    CHECK(lCorner.Position.x == doctest::Approx(10.f + 200.f));
    CHECK(lCorner.Position.y == doctest::Approx(20.f + 150.f));

    // Nothing to anchor on: the size alone.
    const CameraView lEmpty = ZoomAround(lView, 150.f, { 0.f, 0.f }, { 3.f, 4.f });
    CHECK(lEmpty.Position.x == 10.f);
    CHECK(lEmpty.OrthoSize == 150.f);
}

TEST_CASE("CameraView: a rolled view turns the world the other way about its centre")
{
    // Rolled 90 degrees counter-clockwise: what lies to the camera's right (+x) is drawn above the centre.
    const CameraView lView{ { 10.f, 20.f }, 100.f, 90.f };

    const glm::vec4 lRight = MakeView(lView) * glm::vec4(10.f + 30.f, 20.f, 0.f, 1.f);
    CHECK(lRight.x == doctest::Approx(0.f).epsilon(1e-5));
    CHECK(lRight.y == doctest::Approx(-30.f));

    const glm::vec4 lAbove = MakeView(lView) * glm::vec4(10.f, 20.f + 30.f, 0.f, 1.f);
    CHECK(lAbove.x == doctest::Approx(30.f));
    CHECK(lAbove.y == doctest::Approx(0.f).epsilon(1e-5));

    // Unrolled, the view is the plain translation it always was.
    CheckMatrixEqual(MakeView(CameraView{ { 10.f, 20.f }, 100.f, 0.f }),
                     glm::translate(Matrix44F(1.f), glm::vec3(-10.f, -20.f, 0.f)));
}

TEST_CASE("ScreenToWorld and WorldToScreen: a rolled view round-trips, and its screen axes turn with it")
{
    const Vector2F lViewport{ 960.f, 600.f };

    for (const float lRoll : { 0.f, 30.f, -75.f, 90.f, 180.f, 359.f })
    {
        CAPTURE(lRoll);
        const CameraView lView{ { 12.f, -34.f }, 300.f, lRoll };

        for (const Vector2F lPx : { Vector2F{ 0.f, 0.f }, lViewport, Vector2F{ 123.f, 456.f }, lViewport * 0.5f })
        {
            const Vector2F lBack = WorldToScreen(lView, lViewport, ScreenToWorld(lView, lViewport, lPx));
            CHECK(lBack.x == doctest::Approx(lPx.x).epsilon(1e-4));
            CHECK(lBack.y == doctest::Approx(lPx.y).epsilon(1e-4));
        }
    }

    // Rolled 90 degrees: the top of the screen looks toward the world's -x.
    const CameraView lQuarter{ { 0.f, 0.f }, 300.f, 90.f };
    const Vector2F   lTop = ScreenToWorld(lQuarter, lViewport, { 480.f, 0.f });
    CHECK(lTop.x == doctest::Approx(-300.f));
    CHECK(lTop.y == doctest::Approx(0.f).epsilon(1e-4));
}

TEST_CASE("ViewBounds: the world a view covers, a rolled one reaching into its corners")
{
    const Bounds2D lUpright = ViewBounds(CameraView{ { 5.f, 6.f }, 100.f }, 2.f);
    CHECK(lUpright.Center.x == 5.f);
    CHECK(lUpright.Center.y == 6.f);
    CHECK(lUpright.HalfExtent.x == doctest::Approx(200.f));
    CHECK(lUpright.HalfExtent.y == doctest::Approx(100.f));

    // A quarter turn swaps width and height.
    const Bounds2D lQuarter = ViewBounds(CameraView{ { 5.f, 6.f }, 100.f, 90.f }, 2.f);
    CHECK(lQuarter.HalfExtent.x == doctest::Approx(100.f));
    CHECK(lQuarter.HalfExtent.y == doctest::Approx(200.f));

    // Every corner of the rolled view lies inside its bounds.
    const CameraView lRolled{ { 5.f, 6.f }, 100.f, 30.f };
    const Bounds2D   lBounds = ViewBounds(lRolled, 1.6f);
    const Vector2F   lViewport{ 1600.f, 1000.f };
    for (const Vector2F lCorner : { Vector2F{ 0.f, 0.f }, Vector2F{ 1600.f, 0.f }, Vector2F{ 0.f, 1000.f }, lViewport })
    {
        const Vector2F lWorld = ScreenToWorld(lRolled, lViewport, lCorner);
        CHECK(Maths::Abs(lWorld.x - lBounds.Center.x) <= lBounds.HalfExtent.x + 1e-3f);
        CHECK(Maths::Abs(lWorld.y - lBounds.Center.y) <= lBounds.HalfExtent.y + 1e-3f);
    }
}

TEST_CASE("ZoomAround: a rolled view keeps its roll and its anchor")
{
    const CameraView lView{ { 123.f, -45.f }, 300.f, 40.f };
    const Vector2F   lViewport{ 1280.f, 720.f };
    const Vector2F   lAnchor{ 1000.f, 90.f };

    const CameraView lZoomed = ZoomAround(lView, 120.f, lViewport, lAnchor);
    CHECK(lZoomed.Rotation == 40.f);

    const Vector2F lBefore = ScreenToWorld(lView, lViewport, lAnchor);
    const Vector2F lAfter  = ScreenToWorld(lZoomed, lViewport, lAnchor);
    CHECK(lAfter.x == doctest::Approx(lBefore.x).epsilon(1e-5));
    CHECK(lAfter.y == doctest::Approx(lBefore.y).epsilon(1e-5));
}
