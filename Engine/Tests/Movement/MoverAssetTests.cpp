// Suite: the mover assets — MoveMode (a tuning) and Mover (named modes).
// A named mode that is missing gives nullptr (no silent fallback); only "no opinion" gets the default.
// File cases use a unique temp directory, created and removed per case.
#include <doctest.h>

#include "../TestTempDir.h"

#include "Core/IO/FileIO.h"
#include "Movement/Assets/MoveMode.h"
#include "Movement/Assets/Mover.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"

using namespace Opaax;

namespace
{
    struct MovementClasses final : ObjectClassRegistry
    {
        MovementClasses()
        {
            Register<MoveMode>();
            Register<Mover>();
        }
    };

    MoverEntry MakeEntry(const char* InName, const char* InModePath = "")
    {
        MoverEntry lEntry;
        lEntry.Name           = (InName[0] == '\0') ? OpaaxStringID() : OpaaxStringID(InName);
        lEntry.ModeAsset.Path = OpaaxString(InModePath);
        return lEntry;
    }
}

// =============================================================================
// MoveMode — the tuning
// =============================================================================

TEST_CASE("MoveMode: the defaults are a walkable ground mode")
{
    const MoveMode lMode;

    // A tuning that does not load gives these (TAssetHandle::GetOrDefault): a broken reference
    // degrades into walking rather than freezing the entity.
    CHECK(lMode.Mode == OPAAX_ID("GroundMove"));
    CHECK(lMode.MaxSpeed > 0.f);
    CHECK(lMode.JumpSpeed > 0.f);

    const MoveMode* lDefault = static_cast<const MoveMode*>(MoveMode::StaticClass().GetDefaultObject());
    REQUIRE(lDefault != nullptr);
    CHECK(lDefault->Mode == OPAAX_ID("GroundMove"));
}

TEST_CASE("MoveMode: GroundNormalY is the COSINE of the slope limit")
{
    MoveMode lMode;

    // 0 degrees means only a perfectly flat surface counts as ground, so the threshold is 1.
    lMode.MaxSlopeAngleDeg = 0.f;
    CHECK(lMode.GroundNormalY() == doctest::Approx(1.f));

    // 60 degrees admits anything whose normal.y >= 0.5 — the value the sweep compares against.
    lMode.MaxSlopeAngleDeg = 60.f;
    CHECK(lMode.GroundNormalY() == doctest::Approx(0.5f).epsilon(0.001));

    // Steeper limit must ADMIT MORE, i.e. a lower threshold.
    lMode.MaxSlopeAngleDeg = 45.f;
    const float lAt45 = lMode.GroundNormalY();
    lMode.MaxSlopeAngleDeg = 80.f;
    CHECK(lMode.GroundNormalY() < lAt45);
}

TEST_CASE("MoveMode: an editor shows only the fields its mode reads")
{
    MoveMode lMode;
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("JumpSpeed")));
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("GravityScale")));

    // Flight has no ground; the mode stays editable, so it can be switched back.
    lMode.Mode = OPAAX_ID("FlyMove");
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("Mode")));
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("MaxSpeed")));
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("Acceleration")));
    CHECK_FALSE(lMode.IsPropertyVisible(OpaaxStringView("JumpSpeed")));
    CHECK_FALSE(lMode.IsPropertyVisible(OpaaxStringView("GravityScale")));

    // A game's own mode: nothing is hidden.
    lMode.Mode = OPAAX_ID("SwimMove");
    CHECK(lMode.IsPropertyVisible(OpaaxStringView("GravityScale")));
}

TEST_CASE("MoveMode: a tuning round-trips through its asset file; a missing key keeps its default")
{
    const TestTempDir     lDir("modeRoundTrip");
    const MovementClasses lClasses;

    MoveMode lSaved;
    lSaved.MaxSpeed         = 375.f;
    lSaved.JumpSpeed        = 640.f;
    lSaved.MaxSlopeAngleDeg = 60.f;

    const OpaaxString lPath = lDir.Sub("Hero_Ground.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lPath, lClasses);
    const MoveMode* lMode = Cast<MoveMode>(lLoaded.Get());
    REQUIRE(lMode != nullptr);
    CHECK(lMode->MaxSpeed == doctest::Approx(375.f));
    CHECK(lMode->JumpSpeed == doctest::Approx(640.f));
    CHECK(lMode->MaxSlopeAngleDeg == doctest::Approx(60.f));

    // Written before JumpSpeed existed: it still loads, with that field's default.
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString(R"({ "Class": "MoveMode", "Properties": { "Mode": "FlyMove", "MaxSpeed": 900.0 } })")));

    const TStrongObjectPtr<Asset> lOld = AssetFile::LoadAsset(lPartial, lClasses);
    REQUIRE(Cast<MoveMode>(lOld.Get()) != nullptr);
    CHECK(Cast<MoveMode>(lOld.Get())->Mode == OPAAX_ID("FlyMove"));
    CHECK(Cast<MoveMode>(lOld.Get())->JumpSpeed == doctest::Approx(MoveMode{}.JumpSpeed));
}

// =============================================================================
// Mover — the bag, and its resolution rules
// =============================================================================

TEST_CASE("Mover: Find is EXACT for a named mode, and a missing name answers nullptr")
{
    Mover lMover;
    lMover.Entries = { MakeEntry("Ground"), MakeEntry("Fly") };
    lMover.DefaultMode = OPAAX_ID("Ground");

    REQUIRE(lMover.Find(OPAAX_ID("Fly")) != nullptr);
    CHECK(lMover.Find(OPAAX_ID("Fly"))->Name == OPAAX_ID("Fly"));

    // A NAMED mode that is absent does NOT fall back to the default: moving another way for a typo
    // is the wrong-answer failure.
    CHECK(lMover.Find(OPAAX_ID("Swim")) == nullptr);
}

TEST_CASE("Mover: NO OPINION resolves to the default, then to the first entry")
{
    Mover lMover;
    lMover.Entries = { MakeEntry("Ground"), MakeEntry("Fly") };

    lMover.DefaultMode = OPAAX_ID("Fly");
    REQUIRE(lMover.Find(OpaaxStringID{}) != nullptr);
    CHECK(lMover.Find(OpaaxStringID{})->Name == OPAAX_ID("Fly"));

    // A default naming something absent falls through to the first entry rather than to nothing.
    lMover.DefaultMode = OPAAX_ID("Swim");
    REQUIRE(lMover.Find(OpaaxStringID{}) != nullptr);
    CHECK(lMover.Find(OpaaxStringID{})->Name == OPAAX_ID("Ground"));

    const Mover lEmpty;
    CHECK(lEmpty.EntryCount() == 0u);
    CHECK(lEmpty.Find(OpaaxStringID{}) == nullptr);
    CHECK(lEmpty.Find(OPAAX_ID("Ground")) == nullptr);
}

TEST_CASE("Mover: an edit keeps the names usable")
{
    Mover lMover;
    lMover.Entries = {
        MakeEntry("Ground", "Movers/Hero_Ground.opaaxasset"),
        MakeEntry("Ground", "Movers/Hero_Swim.opaaxasset"),   // a repeated name: could never be reached
        MakeEntry("", "Movers/Hero_Fly.opaaxasset"),          // a dropped tuning, no name yet
        MakeEntry("", "Other/Hero_Fly.opaaxasset"),           // its file's name is already taken
    };
    lMover.DefaultMode = OPAAX_ID("Walk");                    // names no entry

    lMover.PostEditChange();

    CHECK(lMover.Entries[0].Name == OPAAX_ID("Ground"));
    CHECK(lMover.Entries[1].Name == OPAAX_ID("Hero_Swim"));   // cleared, then named after its file
    CHECK(lMover.Entries[2].Name == OPAAX_ID("Hero_Fly"));
    CHECK_FALSE(lMover.Entries[3].Name.IsValid());
    CHECK_FALSE(lMover.DefaultMode.IsValid());

    // A valid default stays.
    lMover.DefaultMode = OPAAX_ID("Hero_Fly");
    lMover.PostEditChange();
    CHECK(lMover.DefaultMode == OPAAX_ID("Hero_Fly"));
}

TEST_CASE("Mover: a bag round-trips through its asset file, entries and default alike")
{
    const TestTempDir     lDir("moverRoundTrip");
    const MovementClasses lClasses;

    Mover lSaved;
    lSaved.Entries = { MakeEntry("Ground", "Movers/Hero_Ground.opaaxasset"), MakeEntry("Fly", "Movers/Hero_Fly.opaaxasset") };
    lSaved.DefaultMode = OPAAX_ID("Ground");

    const OpaaxString lPath = lDir.Sub("Hero.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lAsset = AssetFile::LoadAsset(lPath, lClasses);
    const Mover* lLoaded = Cast<Mover>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    REQUIRE(lLoaded->EntryCount() == 2u);
    CHECK(lLoaded->DefaultMode == OPAAX_ID("Ground"));
    CHECK(lLoaded->Entries[0].Name == OPAAX_ID("Ground"));

    // And it still resolves after the round trip, path included.
    REQUIRE(lLoaded->Find(OPAAX_ID("Fly")) != nullptr);
    CHECK(lLoaded->Find(OPAAX_ID("Fly"))->ModeAsset.Path == OpaaxString("Movers/Hero_Fly.opaaxasset"));

    // An empty bag is what a new one is.
    const OpaaxString lEmptyPath = lDir.Sub("Empty.opaaxasset");
    REQUIRE(AssetFile::Save(lEmptyPath, Mover{}));
    const TStrongObjectPtr<Asset> lEmpty = AssetFile::LoadAsset(lEmptyPath, lClasses);
    REQUIRE(Cast<Mover>(lEmpty.Get()) != nullptr);
    CHECK(Cast<Mover>(lEmpty.Get())->EntryCount() == 0u);
}
