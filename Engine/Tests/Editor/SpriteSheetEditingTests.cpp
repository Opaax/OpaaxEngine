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
