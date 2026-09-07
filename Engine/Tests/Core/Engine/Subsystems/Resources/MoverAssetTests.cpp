// Suite: the mover asset PAIR — `.opaaxmovemode` (the tuning) and `.opaaxmover` (the bag that
// names them). ⑦-A P5a.
//
// Deliberately the animation suite's shape, because these are deliberately the animation pair's
// shape: a MoveMode is the CLIP (one tuning, reusable across entities, the unit that grows) and a
// Mover is the LIBRARY (an alias table, so gameplay says "Fly" instead of naming a path).
//
// The resolution rules are what actually matter here, and they are the ones the library got right
// in ⑥ S3: a NAMED mode that is absent answers nullptr rather than falling back, because silently
// moving a different way for a misspelled name is the wrong-answer failure this codebase refuses.
// "I have no opinion" is a different question and is the only one that gets a default.
//
// File cases run against a unique temp directory, created and removed per case — never the repo's
// own assets ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <string>

#include "Core/IO/FileIO.h"   // the malformed-file cases author their own bytes
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeFile.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverFile.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxMoverTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);   // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        OpaaxString File(const char* InName) const
        {
            return OpaaxString((m_Path / InName).string().c_str());
        }

    private:
        fs::path m_Path;
    };

    MoveModeData MakeGround()
    {
        MoveModeData lData;
        lData.Mode             = OPAAX_ID("GroundMove");
        lData.MaxSpeed         = 375.f;
        lData.JumpSpeed        = 640.f;
        lData.MaxSlopeAngleDeg = 60.f;
        return lData;
    }
}

// =============================================================================
// MoveModeData — the tuning
// =============================================================================

TEST_CASE("MoveModeData: the defaults are a walkable ground mode")
{
    const MoveModeData lData;

    // The placeholder policy leans on this: a missing tuning yields something that MOVES, so a
    // broken reference degrades rather than freezing the entity.
    CHECK(lData.Mode == OPAAX_ID("GroundMove"));
    CHECK(lData.MaxSpeed > 0.f);
    CHECK(lData.JumpSpeed > 0.f);
}

TEST_CASE("MoveModeData: GroundNormalY is the COSINE of the slope limit")
{
    MoveModeData lData;

    // 0 degrees means only a perfectly flat surface counts as ground, so the threshold is 1.
    lData.MaxSlopeAngleDeg = 0.f;
    CHECK(lData.GroundNormalY() == doctest::Approx(1.f));

    // 60 degrees admits anything whose normal.y >= 0.5 — the value the sweep compares against.
    lData.MaxSlopeAngleDeg = 60.f;
    CHECK(lData.GroundNormalY() == doctest::Approx(0.5f).epsilon(0.001));

    // Steeper limit must ADMIT MORE, i.e. a lower threshold. A sign error here would invert the
    // whole meaning of the field while still producing a plausible number.
    lData.MaxSlopeAngleDeg = 45.f;
    const float lAt45 = lData.GroundNormalY();
    lData.MaxSlopeAngleDeg = 80.f;
    CHECK(lData.GroundNormalY() < lAt45);
}

TEST_CASE("MoveModeFile: a tuning round-trips through disk unchanged")
{
    const ScopedTempDir lDir("modeRoundTrip");
    const OpaaxString   lPath = lDir.File("Hero_Ground.opaaxmovemode");

    const MoveModeData lSaved = MakeGround();
    REQUIRE(MoveModeFile::Save(lPath, lSaved));

    MoveModeData lLoaded;
    REQUIRE(MoveModeFile::Load(lPath, lLoaded));

    CHECK(lLoaded.Mode == lSaved.Mode);
    CHECK(lLoaded.MaxSpeed == doctest::Approx(lSaved.MaxSpeed));
    CHECK(lLoaded.JumpSpeed == doctest::Approx(lSaved.JumpSpeed));
    CHECK(lLoaded.MaxSlopeAngleDeg == doctest::Approx(lSaved.MaxSlopeAngleDeg));
}

TEST_CASE("MoveModeFile: a file missing a key keeps that field's DEFAULT")
{
    const ScopedTempDir lDir("modePartial");
    const OpaaxString   lPath = lDir.File("Partial.opaaxmovemode");

    // The _WITH_DEFAULT contract, which is what makes "add a field" backward compatible: an asset
    // authored before JumpSpeed existed must still load.
    REQUIRE(FileIO::WriteAllText(lPath, OpaaxString(R"({ "Mode": "FlyMove", "MaxSpeed": 900.0 })")));

    MoveModeData lLoaded;
    REQUIRE(MoveModeFile::Load(lPath, lLoaded));

    CHECK(lLoaded.Mode == OPAAX_ID("FlyMove"));
    CHECK(lLoaded.MaxSpeed == doctest::Approx(900.f));
    CHECK(lLoaded.JumpSpeed == doctest::Approx(MoveModeData{}.JumpSpeed));
}

TEST_CASE("MoveModeFile: a missing or malformed file FAILS and leaves the output untouched")
{
    const ScopedTempDir lDir("modeBad");

    MoveModeData lData = MakeGround();

    CHECK_FALSE(MoveModeFile::Load(lDir.File("NoSuchFile.opaaxmovemode"), lData));
    CHECK(lData.MaxSpeed == doctest::Approx(375.f));   // untouched, not half-written

    const OpaaxString lNotJson = lDir.File("Broken.opaaxmovemode");
    REQUIRE(FileIO::WriteAllText(lNotJson, OpaaxString("this is not json")));
    CHECK_FALSE(MoveModeFile::Load(lNotJson, lData));
    CHECK(lData.MaxSpeed == doctest::Approx(375.f));

    // Valid json that is not an OBJECT — the case a bare array or number would hit.
    const OpaaxString lArray = lDir.File("Array.opaaxmovemode");
    REQUIRE(FileIO::WriteAllText(lArray, OpaaxString("[1, 2, 3]")));
    CHECK_FALSE(MoveModeFile::Load(lArray, lData));
    CHECK(lData.MaxSpeed == doctest::Approx(375.f));
}

// =============================================================================
// MoverData — the bag, and its resolution rules
// =============================================================================

TEST_CASE("MoverData: Find is EXACT for a named mode, and a missing name answers nullptr")
{
    MoverData lData;
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Ground"), {} });
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Fly"), {} });
    lData.DefaultMode = OPAAX_ID("Ground");

    REQUIRE(lData.Find(OPAAX_ID("Fly")) != nullptr);
    CHECK(lData.Find(OPAAX_ID("Fly"))->Name == OPAAX_ID("Fly"));

    // The rule worth pinning: a NAMED mode that is absent does NOT fall back to the default.
    // Silently moving a different way for a typo is the wrong-answer failure, not a convenience.
    CHECK(lData.Find(OPAAX_ID("Swim")) == nullptr);
}

TEST_CASE("MoverData: NO OPINION resolves to the default, then to the first entry")
{
    MoverData lData;
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Ground"), {} });
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Fly"), {} });

    // An invalid id means "I have no opinion" — a different question from a misspelled name.
    lData.DefaultMode = OPAAX_ID("Fly");
    REQUIRE(lData.Find(OpaaxStringID{}) != nullptr);
    CHECK(lData.Find(OpaaxStringID{})->Name == OPAAX_ID("Fly"));

    // A default naming something absent falls through to the first entry rather than to nothing.
    lData.DefaultMode = OPAAX_ID("Swim");
    REQUIRE(lData.Find(OpaaxStringID{}) != nullptr);
    CHECK(lData.Find(OpaaxStringID{})->Name == OPAAX_ID("Ground"));
}

TEST_CASE("MoverData: an EMPTY bag resolves nothing, both ways")
{
    const MoverData lData;

    CHECK(lData.EntryCount() == 0u);
    CHECK(lData.Find(OpaaxStringID{}) == nullptr);
    CHECK(lData.Find(OPAAX_ID("Ground")) == nullptr);
}

TEST_CASE("MoverFile: a bag round-trips, entries and default alike")
{
    const ScopedTempDir lDir("moverRoundTrip");
    const OpaaxString   lPath = lDir.File("Hero.opaaxmover");

    MoverData lSaved;
    lSaved.Entries.emplace_back(MoverEntry{ OPAAX_ID("Ground"), {} });
    lSaved.Entries.back().ModeAsset.Path = "Movers/Hero_Ground.opaaxmovemode";
    lSaved.Entries.emplace_back(MoverEntry{ OPAAX_ID("Fly"), {} });
    lSaved.Entries.back().ModeAsset.Path = "Movers/Hero_Fly.opaaxmovemode";
    lSaved.DefaultMode = OPAAX_ID("Ground");

    REQUIRE(MoverFile::Save(lPath, lSaved));

    MoverData lLoaded;
    REQUIRE(MoverFile::Load(lPath, lLoaded));

    REQUIRE(lLoaded.EntryCount() == 2u);
    CHECK(lLoaded.DefaultMode == OPAAX_ID("Ground"));
    CHECK(lLoaded.Entries[0].Name == OPAAX_ID("Ground"));

    // The PATH is the half that makes the bag a bag — a name with no asset resolves to nothing.
    CHECK(lLoaded.Entries[1].ModeAsset.Path == OpaaxString("Movers/Hero_Fly.opaaxmovemode"));

    // And it still resolves after the round trip, which the raw field compare above does not prove.
    REQUIRE(lLoaded.Find(OPAAX_ID("Fly")) != nullptr);
    CHECK(lLoaded.Find(OPAAX_ID("Fly"))->ModeAsset.Path == OpaaxString("Movers/Hero_Fly.opaaxmovemode"));
}

TEST_CASE("MoverFile: an EMPTY bag saves and loads — it is what a freshly created one is")
{
    const ScopedTempDir lDir("moverEmpty");
    const OpaaxString   lPath = lDir.File("Empty.opaaxmover");

    const MoverData lSaved;
    REQUIRE(MoverFile::Save(lPath, lSaved));

    MoverData lLoaded;
    lLoaded.Entries.emplace_back(MoverEntry{ OPAAX_ID("Stale"), {} });

    REQUIRE(MoverFile::Load(lPath, lLoaded));
    CHECK(lLoaded.EntryCount() == 0u);   // replaced, not merged
}

TEST_CASE("MoverFile: a malformed bag FAILS and leaves the output untouched")
{
    const ScopedTempDir lDir("moverBad");

    MoverData lData;
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Ground"), {} });

    CHECK_FALSE(MoverFile::Load(lDir.File("NoSuchFile.opaaxmover"), lData));
    CHECK(lData.EntryCount() == 1u);

    const OpaaxString lNotJson = lDir.File("Broken.opaaxmover");
    REQUIRE(FileIO::WriteAllText(lNotJson, OpaaxString("{{{")));
    CHECK_FALSE(MoverFile::Load(lNotJson, lData));
    CHECK(lData.EntryCount() == 1u);
}

TEST_CASE("MoverFile: Serialize is EXACTLY what Save writes — the editor's dirty check depends on it")
{
    const ScopedTempDir lDir("moverSerialize");
    const OpaaxString   lPath = lDir.File("Dirty.opaaxmover");

    MoverData lData;
    lData.Entries.emplace_back(MoverEntry{ OPAAX_ID("Ground"), {} });
    lData.DefaultMode = OPAAX_ID("Ground");

    REQUIRE(MoverFile::Save(lPath, lData));

    // If these two ever diverged, an untouched document would report itself dirty forever.
    CHECK(MoverFile::Serialize(lData) == FileIO::ReadAllText(lPath));
}
