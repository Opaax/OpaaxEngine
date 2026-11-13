// Suite: the sprite sheet asset — its frames and default, its file and the old .opaaxsheet format,
// MakeFrameUV and SliceGrid. A wrong V flip would draw a plausible but wrong frame, so the UV maths is
// tested directly. File cases use a unique temp directory, created and removed per case.
#include <doctest.h>

#include "../LegacyAssetFile.h"
#include "../TestTempDir.h"

#include "Core/IO/FileIO.h"
#include "Object/ObjectClassRegistry.h"
#include "Renderer/Textures/SpriteSheet.h"
#include "Resources/Asset/AssetFile.h"

using namespace Opaax;

namespace
{
    struct SheetClasses final : ObjectClassRegistry
    {
        SheetClasses() { Register<SpriteSheet>(); }
    };

    /** Makes OutSheet a 2x2 sheet over a 64x64 texture: cells of 32, no margin, no spacing. */
    void FillProbeSheet(SpriteSheet& OutSheet)
    {
        OutSheet.Texture.Path  = OpaaxString("Textures/Probe.png");
        OutSheet.DefaultFrame  = 2;
        OutSheet.Grid.CellSize = { 32.f, 32.f };
        OutSheet.Frames        = SliceGrid(OutSheet.Grid, 64u, 64u);
    }
}

// =============================================================================
// MakeFrameUV — the V flip, and the degenerates
// =============================================================================
TEST_CASE("MakeFrameUV: the frame's TOP edge becomes the LARGER v")
{
    // A 64x64 texture, the top-left 32x32 cell. Offset.y counts DOWN from the top; GL samples UP
    // from the bottom. So this cell — visually the TOP one — must occupy the UPPER half of v.
    const SpriteFrame  lTopLeft{ OpaaxStringID(), { 0.f, 0.f }, { 32.f, 32.f } };
    const SpriteUVRect lUV = MakeFrameUV(lTopLeft, 64u, 64u);

    CHECK(lUV.UVMin.x == doctest::Approx(0.f));
    CHECK(lUV.UVMax.x == doctest::Approx(0.5f));

    // NOT 0..0.5 — that is exactly the wrong answer a missing flip produces, and it renders the
    // bottom-left cell while looking entirely reasonable.
    CHECK(lUV.UVMin.y == doctest::Approx(0.5f));
    CHECK(lUV.UVMax.y == doctest::Approx(1.f));
}

TEST_CASE("MakeFrameUV: the bottom-right cell is the mirror of the top-left one")
{
    const SpriteFrame  lBottomRight{ OpaaxStringID(), { 32.f, 32.f }, { 32.f, 32.f } };
    const SpriteUVRect lUV = MakeFrameUV(lBottomRight, 64u, 64u);

    CHECK(lUV.UVMin.x == doctest::Approx(0.5f));
    CHECK(lUV.UVMax.x == doctest::Approx(1.f));
    CHECK(lUV.UVMin.y == doctest::Approx(0.f));
    CHECK(lUV.UVMax.y == doctest::Approx(0.5f));
}

TEST_CASE("MakeFrameUV: a frame covering the whole texture is the default 0..1")
{
    // The bridge to every sprite drawn before sheets existed: a full-texture frame must produce
    // exactly the UVs DrawSprite already defaults to, flip included.
    const SpriteFrame  lFull{ OpaaxStringID(), { 0.f, 0.f }, { 128.f, 64.f } };
    const SpriteUVRect lUV = MakeFrameUV(lFull, 128u, 64u);

    CHECK(lUV.UVMin.x == doctest::Approx(0.f));
    CHECK(lUV.UVMin.y == doctest::Approx(0.f));
    CHECK(lUV.UVMax.x == doctest::Approx(1.f));
    CHECK(lUV.UVMax.y == doctest::Approx(1.f));
}

TEST_CASE("MakeFrameUV: degenerate input answers the whole texture, never a division by zero")
{
    const SpriteFrame lFrame{ OpaaxStringID(), { 0.f, 0.f }, { 32.f, 32.f } };

    for (const SpriteUVRect lUV : { MakeFrameUV(lFrame, 0u, 64u),
                                    MakeFrameUV(lFrame, 64u, 0u),
                                    MakeFrameUV(SpriteFrame{}, 64u, 64u) })
    {
        CHECK(lUV.UVMin.x == doctest::Approx(0.f));
        CHECK(lUV.UVMin.y == doctest::Approx(0.f));
        CHECK(lUV.UVMax.x == doctest::Approx(1.f));
        CHECK(lUV.UVMax.y == doctest::Approx(1.f));
    }
}

// =============================================================================
// SliceGrid
// =============================================================================
TEST_CASE("SliceGrid: a 64x64 texture in 32px cells is 4 frames, ROW-MAJOR")
{
    SpriteSheetGrid lGrid;
    lGrid.CellSize = { 32.f, 32.f };

    const TDynArray<SpriteFrame> lFrames = SliceGrid(lGrid, 64u, 64u);

    REQUIRE(lFrames.size() == 4u);

    // Row-major is what a frame INDEX means everywhere else, so it is asserted rather than assumed:
    // index 1 is the cell to the RIGHT of index 0, not the one below it.
    CHECK(lFrames[0].Offset.x == doctest::Approx(0.f));
    CHECK(lFrames[0].Offset.y == doctest::Approx(0.f));
    CHECK(lFrames[1].Offset.x == doctest::Approx(32.f));
    CHECK(lFrames[1].Offset.y == doctest::Approx(0.f));
    CHECK(lFrames[2].Offset.x == doctest::Approx(0.f));
    CHECK(lFrames[2].Offset.y == doctest::Approx(32.f));

    // Generated frames have no name (a name per cell would intern strings for nothing).
    CHECK_FALSE(lFrames[0].Name.IsValid());
}

TEST_CASE("SliceGrid: margin and spacing move every cell, and the overhang is DROPPED")
{
    SpriteSheetGrid lGrid;
    lGrid.CellSize = { 16.f, 16.f };
    lGrid.Margin   = { 2.f, 2.f };
    lGrid.Spacing  = { 4.f, 4.f };

    // 2 + 16 + 4 + 16 = 38 fits in 40; a third cell would need 58.
    const TDynArray<SpriteFrame> lFrames = SliceGrid(lGrid, 40u, 40u);

    REQUIRE(lFrames.size() == 4u);
    CHECK(lFrames[0].Offset.x == doctest::Approx(2.f));
    CHECK(lFrames[1].Offset.x == doctest::Approx(22.f));
    CHECK(lFrames[3].Offset.y == doctest::Approx(22.f));
}

TEST_CASE("SliceGrid: an explicit count asking for more than fits yields only what fits")
{
    SpriteSheetGrid lGrid;
    lGrid.CellSize = { 32.f, 32.f };
    lGrid.Columns  = 8;   // 8 * 32 = 256, on a 64px-wide texture
    lGrid.Rows     = 1;

    CHECK(SliceGrid(lGrid, 64u, 32u).size() == 2u);
}

TEST_CASE("SliceGrid: a zero cell or a zero texture slices nothing")
{
    SpriteSheetGrid lZeroCell;
    lZeroCell.CellSize = { 0.f, 32.f };

    SpriteSheetGrid lGrid;
    lGrid.CellSize = { 32.f, 32.f };

    CHECK(SliceGrid(lZeroCell, 64u, 64u).empty());
    CHECK(SliceGrid(lGrid, 0u, 64u).empty());
}


// =============================================================================
// SpriteSheet::FrameAt and FindFrameIndex — where a sprite's index and a clip's name resolve
// =============================================================================
TEST_CASE("FrameAt: a negative index means the sheet's own DefaultFrame")
{
    SpriteSheet lSheet;
    FillProbeSheet(lSheet);   // DefaultFrame = 2

    REQUIRE(lSheet.FrameAt(-1) != nullptr);
    CHECK(lSheet.FrameAt(-1)->Offset.y == doctest::Approx(32.f));   // frame 2 = second row
    CHECK(lSheet.FrameAt(-1) == lSheet.FrameAt(2));
}

TEST_CASE("FrameAt: out of range answers NULL rather than clamping")
{
    // Null lets RendererManager warn before falling back (no silent wrong frame).
    SpriteSheet lSheet;
    FillProbeSheet(lSheet);

    CHECK(lSheet.FrameAt(4)   == nullptr);
    CHECK(lSheet.FrameAt(999) == nullptr);

    const SpriteSheet lEmpty;
    CHECK(lEmpty.FrameAt(-1) == nullptr);   // no frames, so not even the default resolves
    CHECK(lEmpty.FrameAt(0)  == nullptr);
}

TEST_CASE("FindFrameIndex: a name finds its first frame; an unknown or empty name finds none")
{
    SpriteSheet lSheet;
    FillProbeSheet(lSheet);
    lSheet.Frames[1].Name = OPAAX_ID("Run");
    lSheet.Frames[3].Name = OPAAX_ID("Run");   // clips bind the first of two

    CHECK(lSheet.FindFrameIndex(OPAAX_ID("Run")) == 1);
    CHECK(lSheet.FindFrameIndex(OPAAX_ID("Jump")) == -1);

    // Not the unnamed frames: an empty name is "no frame", never "frame 0".
    CHECK(lSheet.FindFrameIndex(OpaaxStringID()) == -1);
}

TEST_CASE("An edit leaving the default past the last frame sets it back to the first")
{
    SpriteSheet lSheet;
    FillProbeSheet(lSheet);

    lSheet.Frames.resize(2);   // the default, frame 2, is gone
    lSheet.PostEditChange();
    CHECK(lSheet.DefaultFrame == 0u);

    // One still in range is kept.
    lSheet.DefaultFrame = 1u;
    lSheet.PostEditChange();
    CHECK(lSheet.DefaultFrame == 1u);
}

// =============================================================================
// The slice border — kept inside its frame
// =============================================================================
TEST_CASE("ClampBorderInside: a border that fits is kept as it is")
{
    Margin lBorder{ 112.f, 112.f, 85.f, 112.f };
    ClampBorderInside(lBorder, { 256.f, 256.f });

    CHECK(lBorder.Left == doctest::Approx(112.f));
    CHECK(lBorder.Right == doctest::Approx(112.f));
    CHECK(lBorder.Bottom == doctest::Approx(85.f));
    CHECK(lBorder.Top == doctest::Approx(112.f));
}

TEST_CASE("ClampBorderInside: no negative side, and two opposite sides shrink to the frame's size")
{
    Margin lBorder{ 200.f, 200.f, -5.f, 300.f };
    ClampBorderInside(lBorder, { 256.f, 100.f });

    // Left and right in proportion: 200 + 200 over 256 gives 128 each.
    CHECK(lBorder.Left == doctest::Approx(128.f));
    CHECK(lBorder.Right == doctest::Approx(128.f));
    // Bottom is not negative, and the top alone fills the height.
    CHECK(lBorder.Bottom == doctest::Approx(0.f));
    CHECK(lBorder.Top == doctest::Approx(100.f));
}

TEST_CASE("ClampBorderInside: a frame with no size (the whole texture) only loses negatives")
{
    Margin lBorder{ -1.f, 600.f, 20.f, 20.f };
    ClampBorderInside(lBorder, { 0.f, 0.f });

    CHECK(lBorder.Left == doctest::Approx(0.f));
    CHECK(lBorder.Right == doctest::Approx(600.f));
    CHECK(lBorder.Bottom == doctest::Approx(20.f));
}

TEST_CASE("An edit leaving a border larger than its frame brings it back inside")
{
    SpriteSheet lSheet;
    FillProbeSheet(lSheet);   // 32 px frames
    lSheet.Frames[1].Border = Margin{ 30.f, 30.f, 8.f, 8.f };
    lSheet.Frames[2].Border = Margin{ 4.f, 4.f, 4.f, 4.f };

    lSheet.PostEditChange();

    CHECK(lSheet.Frames[1].Border.Left == doctest::Approx(16.f));
    CHECK(lSheet.Frames[1].Border.Right == doctest::Approx(16.f));
    CHECK(lSheet.Frames[1].Border.Top == doctest::Approx(8.f));
    CHECK(lSheet.Frames[2].Border.Left == doctest::Approx(4.f));   // one that fits is untouched
    CHECK(lSheet.Frames[0].Border.IsZero());
}

// =============================================================================
// The file
// =============================================================================
TEST_CASE("SpriteSheet: save then load round-trips every field")
{
    const TestTempDir  lDir("sheetRoundTrip");
    const SheetClasses lClasses;
    const OpaaxString  lPath = lDir.Sub("Hero.opaaxasset");

    SpriteSheet lSaved;
    FillProbeSheet(lSaved);
    lSaved.Frames[1].Name   = OPAAX_ID("Hero_Idle_1");
    lSaved.Frames[2].Border = Margin{ 12.f, 11.f, 9.f, 10.f };
    lSaved.Frames[2].SliceMode = ESliceMode::Tile;

    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
    const SpriteSheet*            lLoaded = Cast<SpriteSheet>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);

    CHECK(lLoaded->Texture.Path == lSaved.Texture.Path);
    CHECK(lLoaded->DefaultFrame == lSaved.DefaultFrame);
    CHECK(lLoaded->FrameCount() == lSaved.FrameCount());
    CHECK(lLoaded->Grid.CellSize.x == doctest::Approx(lSaved.Grid.CellSize.x));

    // The name survives as an ID that COMPARES EQUAL — the interned round trip, not just the text.
    CHECK(lLoaded->Frames[1].Name == OPAAX_ID("Hero_Idle_1"));
    CHECK_FALSE(lLoaded->Frames[0].Name.IsValid());
    CHECK(lLoaded->Frames[3].Offset.x == doctest::Approx(32.f));

    // The slice border, each side in its own place.
    CHECK(lLoaded->Frames[2].Border.Left == doctest::Approx(12.f));
    CHECK(lLoaded->Frames[2].Border.Right == doctest::Approx(11.f));
    CHECK(lLoaded->Frames[2].Border.Bottom == doctest::Approx(9.f));
    CHECK(lLoaded->Frames[2].Border.Top == doctest::Approx(10.f));
    CHECK(lLoaded->Frames[0].Border.IsZero());
    CHECK(lLoaded->Frames[2].SliceMode == ESliceMode::Tile);
    CHECK(lLoaded->Frames[0].SliceMode == ESliceMode::Stretch);

    // Saving what was loaded writes the same text: the editor's dirty check compares it.
    CHECK(AssetFile::Serialize(*lLoaded) == AssetFile::Serialize(lSaved));
}

TEST_CASE("SpriteSheet: a file missing keys keeps the defaults; a wrong type is refused")
{
    const TestTempDir  lDir("sheetPartial");
    const SheetClasses lClasses;

    // Adding a field must not break sheets saved before it.
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString(R"({ "Class": "SpriteSheet", "Properties": { "DefaultFrame": 3 } })")));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPartial, lClasses);
    const SpriteSheet*            lLoaded = Cast<SpriteSheet>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    CHECK(lLoaded->DefaultFrame == 3u);
    CHECK(lLoaded->Frames.empty());
    CHECK(lLoaded->Grid.CellSize.x == doctest::Approx(32.f));   // the struct's own default

    // A sheet saved before frames had a border: its frames load unsliced.
    const OpaaxString lBeforeBorder = lDir.Sub("BeforeBorder.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lBeforeBorder, OpaaxString(R"({ "Class": "SpriteSheet", "Properties": {
        "Frames": [ { "Name": "Idle", "Offset": { "x": 0.0, "y": 0.0 }, "Size": { "x": 64.0, "y": 64.0 } } ] } })")));

    const TStrongObjectPtr<Asset> lOld      = AssetFile::LoadAsset(lBeforeBorder, lClasses);
    const SpriteSheet*            lOldSheet = Cast<SpriteSheet>(lOld.Get());
    REQUIRE(lOldSheet != nullptr);
    REQUIRE(lOldSheet->FrameCount() == 1u);
    CHECK(lOldSheet->Frames[0].Size.x == doctest::Approx(64.f));
    CHECK(lOldSheet->Frames[0].Border.IsZero());
    CHECK(lOldSheet->Frames[0].SliceMode == ESliceMode::Stretch);

    const OpaaxString lWrongType = lDir.Sub("WrongType.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lWrongType, OpaaxString(R"({ "Class": "SpriteSheet", "Properties": { "DefaultFrame": "three" } })")));
    CHECK(AssetFile::LoadAsset(lWrongType, lClasses).Get() == nullptr);
}

TEST_CASE("SpriteSheet: an old .opaaxsheet converts with its texture, frames and grid")
{
    const TestTempDir  lDir("sheetConvert");
    const SheetClasses lClasses;

    const TStrongObjectPtr<Asset> lAsset = ConvertAndLoad(lDir, "Sheets/Hero.opaaxsheet", R"({
        "Texture": "Textures/Hero.png", "DefaultFrame": 1,
        "Frames": [ { "Name": "Idle", "Offset": { "x": 0.0, "y": 0.0 }, "Size": { "x": 64.0, "y": 64.0 } },
                    { "Name": "Run_0", "Offset": { "x": 64.0, "y": 0.0 }, "Size": { "x": 64.0, "y": 64.0 } } ],
        "Grid": { "CellSize": { "x": 64.0, "y": 64.0 } } })", lClasses);

    const SpriteSheet* lSheet = Cast<SpriteSheet>(lAsset.Get());
    REQUIRE(lSheet != nullptr);
    CHECK(lSheet->Texture.Path == "Textures/Hero.png");
    CHECK(lSheet->DefaultFrame == 1u);
    REQUIRE(lSheet->FrameCount() == 2u);
    CHECK(lSheet->Frames[1].Name == OPAAX_ID("Run_0"));
    CHECK(lSheet->Frames[1].Offset.x == doctest::Approx(64.f));
    CHECK(lSheet->Frames[1].Border.IsZero());   // the old format had no border
    CHECK(lSheet->Grid.CellSize.x == doctest::Approx(64.f));
}
