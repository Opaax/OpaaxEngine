// Suite: the sprite sheet ASSET — its data, its `.opaaxsheet` file, and the two pure functions the
// whole feature stands on (MakeFrameUV, SliceGrid).
//
// Both of those are free and pure precisely so they can be tested with no GL context, the way
// MakeSortKey and MakeOutlineInnerHalf are — and the UV one earns it: getting the V flip backwards
// draws a plausible-looking WRONG frame rather than failing, which is the failure class this
// codebase treats as its worst.
//
// The file cases run against a unique temp directory, created and removed per case — never the
// repo's own assets ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <string>

#include "Core/IO/FileIO.h"   // the malformed-file cases author their own bytes
#include "Engine/Subsystems/Resources/Types/SpriteSheetData.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetFile.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxSheetTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);          // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        OpaaxString Sub(const char* InRel) const
        {
            return OpaaxString((m_Path / InRel).generic_string().c_str());
        }

    private:
        fs::path m_Path;
    };

    /** A 2x2 sheet over a 64x64 texture: cells of 32, no margin, no spacing. */
    SpriteSheetData MakeProbeSheet()
    {
        SpriteSheetData lData;
        lData.Texture.Path  = OpaaxString("Textures/Probe.png");
        lData.DefaultFrame  = 2;
        lData.Grid.CellSize = { 32.f, 32.f };
        lData.Frames        = SliceGrid(lData.Grid, 64u, 64u);

        return lData;
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

    // Generated frames are UNNAMED — a "Frame_12" per cell would intern a string for the life of
    // the process to say what the index already says.
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
// SpriteSheetData::FrameAt — where the -1 sentinel is resolved, once
// =============================================================================
TEST_CASE("FrameAt: a negative index means the sheet's own DefaultFrame")
{
    const SpriteSheetData lSheet = MakeProbeSheet();   // DefaultFrame = 2

    REQUIRE(lSheet.FrameAt(-1) != nullptr);
    CHECK(lSheet.FrameAt(-1)->Offset.y == doctest::Approx(32.f));   // frame 2 = second row
    CHECK(lSheet.FrameAt(-1) == lSheet.FrameAt(2));
}

TEST_CASE("FrameAt: out of range answers NULL rather than clamping")
{
    // Deliberate: a caller that silently drew a different frame would be the silent-wrong-answer
    // failure. Null is what lets RendererManager say so before falling back.
    const SpriteSheetData lSheet = MakeProbeSheet();

    CHECK(lSheet.FrameAt(4)   == nullptr);
    CHECK(lSheet.FrameAt(999) == nullptr);

    SpriteSheetData lEmpty;
    CHECK(lEmpty.FrameAt(-1) == nullptr);   // no frames, so not even the default resolves
    CHECK(lEmpty.FrameAt(0)  == nullptr);
}

// =============================================================================
// The file
// =============================================================================
TEST_CASE("SpriteSheetFile: save then load round-trips every field")
{
    const ScopedTempDir   lDir("roundtrip");
    const OpaaxString     lPath  = lDir.Sub("Hero.opaaxsheet");
    SpriteSheetData       lSaved = MakeProbeSheet();

    lSaved.Frames[1].Name = OPAAX_ID("Hero_Idle_1");

    REQUIRE(SpriteSheetFile::Save(lPath, lSaved));

    SpriteSheetData lLoaded;
    REQUIRE(SpriteSheetFile::Load(lPath, lLoaded));

    CHECK(lLoaded.Texture.Path == lSaved.Texture.Path);
    CHECK(lLoaded.DefaultFrame == lSaved.DefaultFrame);
    CHECK(lLoaded.FrameCount() == lSaved.FrameCount());
    CHECK(lLoaded.Grid.CellSize.x == doctest::Approx(lSaved.Grid.CellSize.x));

    // The name survives as an ID that COMPARES EQUAL — the interned round trip, not just the text.
    CHECK(lLoaded.Frames[1].Name == OPAAX_ID("Hero_Idle_1"));
    CHECK_FALSE(lLoaded.Frames[0].Name.IsValid());
    CHECK(lLoaded.Frames[3].Offset.x == doctest::Approx(32.f));
}

TEST_CASE("SpriteSheetFile: the text a save writes is the text a save writes again")
{
    // What the editor's dirty marker stands on: Serialize must be stable, or a `*` appears on a
    // sheet nobody touched (the trap L30 records for the map baseline).
    const SpriteSheetData lData = MakeProbeSheet();

    CHECK(SpriteSheetFile::Serialize(lData) == SpriteSheetFile::Serialize(lData));

    const ScopedTempDir lDir("stable");
    const OpaaxString   lPath = lDir.Sub("Stable.opaaxsheet");
    REQUIRE(SpriteSheetFile::Save(lPath, lData));

    SpriteSheetData lLoaded;
    REQUIRE(SpriteSheetFile::Load(lPath, lLoaded));

    CHECK(SpriteSheetFile::Serialize(lLoaded) == SpriteSheetFile::Serialize(lData));
}

TEST_CASE("SpriteSheetFile: a missing or malformed file leaves the caller's data untouched")
{
    const ScopedTempDir lDir("bad");

    SpriteSheetData lExisting = MakeProbeSheet();

    CHECK_FALSE(SpriteSheetFile::Load(lDir.Sub("NoSuchFile.opaaxsheet"), lExisting));
    CHECK(lExisting.FrameCount() == 4u);   // not half-overwritten

    // Valid json, wrong shape: an array is not a sheet.
    const OpaaxString lArrayPath = lDir.Sub("Array.opaaxsheet");
    REQUIRE(FileIO::WriteAllText(lArrayPath, OpaaxString("[1, 2, 3]")));

    CHECK_FALSE(SpriteSheetFile::Load(lArrayPath, lExisting));
    CHECK(lExisting.FrameCount() == 4u);
}

TEST_CASE("SpriteSheetFile: a file missing keys keeps the defaults (_WITH_DEFAULT), a wrong TYPE fails")
{
    const ScopedTempDir lDir("partial");

    // Adding a field must never refuse a sheet written before it existed — the same rule that keeps
    // every .opaaxmap loading (I8).
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxsheet");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString("{\n    \"DefaultFrame\": 3\n}")));

    SpriteSheetData lLoaded;
    REQUIRE(SpriteSheetFile::Load(lPartial, lLoaded));
    CHECK(lLoaded.DefaultFrame == 3u);
    CHECK(lLoaded.Frames.empty());
    CHECK(lLoaded.Grid.CellSize.x == doctest::Approx(32.f));   // the struct's own default

    // A hand-edited value of the wrong type is caught rather than thrown at the caller.
    const OpaaxString lWrongType = lDir.Sub("WrongType.opaaxsheet");
    REQUIRE(FileIO::WriteAllText(lWrongType, OpaaxString("{\n    \"DefaultFrame\": \"three\"\n}")));

    SpriteSheetData lUntouched;
    CHECK_FALSE(SpriteSheetFile::Load(lWrongType, lUntouched));
}
