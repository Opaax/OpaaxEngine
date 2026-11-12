// Suite: what the sprite sheet editor does to a sheet — slicing it by its grid, naming its frames, and
// finding the frame a click lands on (Editor/Assets/Editors/SpriteSheetEditing.h).
#include <doctest.h>

#include "Editor/Assets/Editors/SpriteSheetEditing.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    SpriteFrame MakeFrame(const char* InName, const Vector2F InOffset, const Vector2F InSize)
    {
        SpriteFrame lFrame;
        lFrame.Name   = (InName[0] == '\0') ? OpaaxStringID() : OpaaxStringID(InName);
        lFrame.Offset = InOffset;
        lFrame.Size   = InSize;
        return lFrame;
    }
}

TEST_CASE("SheetEditing: Slice replaces the frames with the grid's cells, and keeps a default that still exists")
{
    SpriteSheet lSheet;
    lSheet.Frames        = { MakeFrame("Old", { 0.f, 0.f }, { 8.f, 8.f }) };
    lSheet.Grid.CellSize = { 32.f, 32.f };
    lSheet.DefaultFrame  = 0u;

    REQUIRE(SheetEditing::Slice(lSheet, 64u, 64u));
    REQUIRE(lSheet.FrameCount() == 4u);
    CHECK_FALSE(lSheet.Frames[0].Name.IsValid());   // the old frame is gone
    CHECK(lSheet.DefaultFrame == 0u);

    // A default past the new last frame goes back to the first.
    lSheet.DefaultFrame  = 3u;
    lSheet.Grid.CellSize = { 64.f, 64.f };
    REQUIRE(SheetEditing::Slice(lSheet, 64u, 64u));
    CHECK(lSheet.FrameCount() == 1u);
    CHECK(lSheet.DefaultFrame == 0u);
}

TEST_CASE("SheetEditing: a grid that cuts nothing is refused, and the frames are kept")
{
    // A cell larger than the texture: a typo, not a wish to lose every frame.
    SpriteSheet lSheet;
    lSheet.Frames        = { MakeFrame("Idle", { 0.f, 0.f }, { 32.f, 32.f }) };
    lSheet.Grid.CellSize = { 128.f, 128.f };

    CHECK_FALSE(SheetEditing::Slice(lSheet, 64u, 64u));
    REQUIRE(lSheet.FrameCount() == 1u);
    CHECK(lSheet.Frames[0].Name == OPAAX_ID("Idle"));
}

TEST_CASE("SheetEditing: Auto-Name names the unnamed frames by index, skips names in use, keeps the rest")
{
    TDynArray<SpriteFrame> lFrames = { MakeFrame("", { 0.f, 0.f }, { 1.f, 1.f }),
                                       MakeFrame("Frame_2", { 1.f, 0.f }, { 1.f, 1.f }),   // an author's name
                                       MakeFrame("", { 2.f, 0.f }, { 1.f, 1.f }) };

    CHECK(SheetEditing::AutoNameFrames(lFrames) == 2u);
    CHECK(lFrames[0].Name == OPAAX_ID("Frame_0"));
    CHECK(lFrames[1].Name == OPAAX_ID("Frame_2"));
    CHECK(lFrames[2].Name == OPAAX_ID("Frame_3"));   // "Frame_2" is taken: a clip would bind the first

    // A second press changes nothing.
    CHECK(SheetEditing::AutoNameFrames(lFrames) == 0u);
    CHECK(lFrames[2].Name == OPAAX_ID("Frame_3"));
}

TEST_CASE("SheetEditing: a click finds the first frame under it, edges included")
{
    const TDynArray<SpriteFrame> lFrames = { MakeFrame("A", { 0.f, 0.f }, { 32.f, 32.f }),
                                             MakeFrame("B", { 16.f, 16.f }, { 32.f, 32.f }) };

    CHECK(SheetEditing::FrameUnder(lFrames, { 4.f, 4.f }) == 0);
    CHECK(SheetEditing::FrameUnder(lFrames, { 20.f, 20.f }) == 0);   // both hold it: the first one
    CHECK(SheetEditing::FrameUnder(lFrames, { 40.f, 40.f }) == 1);
    CHECK(SheetEditing::FrameUnder(lFrames, { 48.f, 48.f }) == 1);   // B's corner
    CHECK(SheetEditing::FrameUnder(lFrames, { 60.f, 2.f }) == -1);
}

// =============================================================================
// The slice border's lines: which a press takes, and where a drag puts them
// =============================================================================
TEST_CASE("SheetEditing: a press near one border line takes it; near a crossing it takes both; the middle takes none")
{
    const Margin   lBorder{ 112.f, 112.f, 85.f, 112.f };   // lines at x 112 and 144, y 112 and 171
    const Vector2F lSize{ 256.f, 256.f };

    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 114.f, 30.f }, 3.f)  == ERectEdge::Left);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 142.f, 30.f }, 3.f)  == ERectEdge::Right);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 30.f, 110.f }, 3.f)  == ERectEdge::Top);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 30.f, 172.f }, 3.f)  == ERectEdge::Bottom);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 113.f, 111.f }, 3.f) == ERectEdge::TopLeft);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 145.f, 170.f }, 3.f) == ERectEdge::BottomRight);

    // Between the lines, and outside the frame, nothing.
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 128.f, 140.f }, 3.f) == ERectEdge::None);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 30.f, 30.f }, 3.f)   == ERectEdge::None);
    CHECK(SheetEditing::BorderLinesUnder(lBorder, lSize, { 113.f, 300.f }, 3.f) == ERectEdge::None);
}

TEST_CASE("SheetEditing: a zero border's lines lie on the frame's edges and are taken there")
{
    const Margin   lNone;
    const Vector2F lSize{ 64.f, 32.f };

    CHECK(SheetEditing::BorderLinesUnder(lNone, lSize, { 1.f, 16.f }, 2.f)  == ERectEdge::Left);
    CHECK(SheetEditing::BorderLinesUnder(lNone, lSize, { 63.f, 16.f }, 2.f) == ERectEdge::Right);
    CHECK(SheetEditing::BorderLinesUnder(lNone, lSize, { 32.f, 0.f }, 2.f)  == ERectEdge::Top);
    CHECK(SheetEditing::BorderLinesUnder(lNone, lSize, { 0.f, 32.f }, 2.f)  == ERectEdge::BottomLeft);

    // Dragged inwards, a zero line becomes a border.
    const Margin lDragged = SheetEditing::DragBorder(lNone, lSize, ERectEdge::Left, { 9.3f, 16.f });
    CHECK(lDragged.Left == doctest::Approx(9.f));
    CHECK(lDragged.Right == doctest::Approx(0.f));
}

TEST_CASE("SheetEditing: a dragged line lands on whole pixels, inside the frame, and stops at the opposite line")
{
    const Margin   lBorder{ 112.f, 112.f, 85.f, 112.f };
    const Vector2F lSize{ 256.f, 256.f };

    // The cursor's pixel, rounded.
    Margin lLeft = SheetEditing::DragBorder(lBorder, lSize, ERectEdge::Left, { 100.6f, 50.f });
    CHECK(lLeft.Left == doctest::Approx(101.f));
    CHECK(lLeft.Right == doctest::Approx(112.f));   // the others are kept
    CHECK(lLeft.Top == doctest::Approx(112.f));

    // Right and bottom count from their own edge.
    const Margin lRight = SheetEditing::DragBorder(lBorder, lSize, ERectEdge::BottomRight, { 200.2f, 230.f });
    CHECK(lRight.Right == doctest::Approx(56.f));
    CHECK(lRight.Bottom == doctest::Approx(26.f));
    CHECK(lRight.Left == doctest::Approx(112.f));

    // Never past the opposite line: Left stops at 256 - 112 = 144.
    lLeft = SheetEditing::DragBorder(lBorder, lSize, ERectEdge::Left, { 220.f, 50.f });
    CHECK(lLeft.Left == doctest::Approx(144.f));

    // Never outside the frame.
    lLeft = SheetEditing::DragBorder(lBorder, lSize, ERectEdge::TopLeft, { -40.f, -5.f });
    CHECK(lLeft.Left == doctest::Approx(0.f));
    CHECK(lLeft.Top == doctest::Approx(0.f));
    const Margin lOut = SheetEditing::DragBorder(lBorder, lSize, ERectEdge::Bottom, { 50.f, 400.f });
    CHECK(lOut.Bottom == doctest::Approx(0.f));
}

// =============================================================================
// The sliced preview
// =============================================================================
namespace
{
    /** Bhop's case: the 256 px Normal cell at (256, 0) of a 512x512 sheet, a 112/112/85/112 border. */
    SpriteFrame BracketFrame()
    {
        SpriteFrame lFrame = MakeFrame("Normal", { 256.f, 0.f }, { 256.f, 256.f });
        lFrame.Border      = Margin{ 112.f, 112.f, 85.f, 112.f };
        return lFrame;
    }

    const SheetEditing::PreviewQuad* PreviewAt(const TDynArray<SheetEditing::PreviewQuad>& InQuads, const Vector2F InMin)
    {
        for (const SheetEditing::PreviewQuad& lQuad : InQuads)
        {
            if (Maths::Abs(lQuad.Min.x - InMin.x) < 0.01f && Maths::Abs(lQuad.Min.y - InMin.y) < 0.01f)
            {
                return &lQuad;
            }
        }
        return nullptr;
    }
}

TEST_CASE("SheetEditing: the preview's quads are the UI's slice, one for one, turned Y down")
{
    const SpriteFrame lFrame = BracketFrame();

    const TDynArray<SheetEditing::PreviewQuad> lPreview =
        SheetEditing::PreviewQuads(lFrame, { 512.f, 512.f }, { 420.f, 90.f }, 0.4f);

    TDynArray<UIQuad> lUI;
    BuildSlicedQuads(Bounds2D::FromMinMax({ 0.f, 0.f }, { 420.f, 90.f }), lFrame.Border, 0.4f, lFrame.Size, lUI);

    REQUIRE(lPreview.size() == 9u);
    REQUIRE(lPreview.size() == lUI.size());
    for (Uint64 lIndex = 0; lIndex < lUI.size(); ++lIndex)
    {
        CAPTURE(lIndex);
        CHECK(lPreview[lIndex].Max.x - lPreview[lIndex].Min.x == doctest::Approx(lUI[lIndex].Bounds.Size().x));
        CHECK(lPreview[lIndex].Max.y - lPreview[lIndex].Min.y == doctest::Approx(lUI[lIndex].Bounds.Size().y));
        CHECK(lPreview[lIndex].Min.y == doctest::Approx(90.f - lUI[lIndex].Bounds.Max().y));
    }
}

TEST_CASE("SheetEditing: the preview's top-left quad shows the frame's top-left pixels, at the scale")
{
    const SpriteFrame lFrame = BracketFrame();

    const TDynArray<SheetEditing::PreviewQuad> lQuads =
        SheetEditing::PreviewQuads(lFrame, { 512.f, 512.f }, { 420.f, 90.f }, 0.4f);

    const SheetEditing::PreviewQuad* lTopLeft = PreviewAt(lQuads, { 0.f, 0.f });
    REQUIRE(lTopLeft != nullptr);
    CHECK(lTopLeft->Max.x == doctest::Approx(112.f * 0.4f));
    CHECK(lTopLeft->Max.y == doctest::Approx(112.f * 0.4f));   // the top border, not the bottom one
    CHECK(lTopLeft->UVMin.x == doctest::Approx(256.f / 512.f));
    CHECK(lTopLeft->UVMin.y == doctest::Approx(0.f));
    CHECK(lTopLeft->UVMax.x == doctest::Approx((256.f + 112.f) / 512.f));
    CHECK(lTopLeft->UVMax.y == doctest::Approx(112.f / 512.f));

    // The bottom-right one shows the frame's bottom-right pixels.
    const SheetEditing::PreviewQuad* lBottomRight = PreviewAt(lQuads, { 420.f - 112.f * 0.4f, 90.f - 85.f * 0.4f });
    REQUIRE(lBottomRight != nullptr);
    CHECK(lBottomRight->UVMax.x == doctest::Approx(1.f));
    CHECK(lBottomRight->UVMax.y == doctest::Approx(256.f / 512.f));
    CHECK(lBottomRight->UVMin.y == doctest::Approx((256.f - 85.f) / 512.f));
}

TEST_CASE("SheetEditing: a frame with no border previews as one quad over the whole rect")
{
    const SpriteFrame lFrame = MakeFrame("Hatch", { 0.f, 256.f }, { 256.f, 256.f });

    const TDynArray<SheetEditing::PreviewQuad> lQuads =
        SheetEditing::PreviewQuads(lFrame, { 512.f, 512.f }, { 420.f, 90.f }, 1.f);

    REQUIRE(lQuads.size() == 1u);
    CHECK(lQuads[0].Max.x == doctest::Approx(420.f));
    CHECK(lQuads[0].Max.y == doctest::Approx(90.f));
    CHECK(lQuads[0].UVMin.y == doctest::Approx(0.5f));
    CHECK(lQuads[0].UVMax.y == doctest::Approx(1.f));

    CHECK(SheetEditing::PreviewQuads(lFrame, { 512.f, 512.f }, { 0.f, 90.f }, 1.f).empty());
}
