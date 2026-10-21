// Suite: the mover assets — the move modes (a way of moving and its tuning) and Mover (named modes).
// A named mode that is missing gives nullptr (no silent fallback); only "no opinion" gets the default.
// File cases use a unique temp directory, created and removed per case.
#include <doctest.h>

#include "../TestTempDir.h"

#include "Core/IO/FileIO.h"
#include "Movement/Assets/MoveMode.h"
#include "Movement/Assets/Mover.h"
#include "Movement/Modes/FlyMoveMode.h"
#include "Movement/Modes/GroundMoveMode.h"
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
            Register<GroundMoveMode>();
            Register<FlyMoveMode>();
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
// The modes — a way of moving and its tuning
// =============================================================================

TEST_CASE("GroundMoveMode: the defaults walk and jump")
{
    const GroundMoveMode lMode;

    // What a mode that does not load moves with (MoverSubsystem falls back to this class's default
    // object): a broken reference degrades into walking rather than freezing the entity.
    CHECK(lMode.MaxSpeed > 0.f);
    CHECK(lMode.JumpSpeed > 0.f);
    CHECK(lMode.GravityScale == doctest::Approx(1.f));

    const GroundMoveMode* lDefault = static_cast<const GroundMoveMode*>(GroundMoveMode::StaticClass().GetDefaultObject());
    REQUIRE(lDefault != nullptr);
    CHECK(lDefault->MaxSpeed == doctest::Approx(lMode.MaxSpeed));
}

TEST_CASE("MoveMode: GroundNormalY is the COSINE of the slope limit")
{
    FlyMoveMode lMode;   // the base's, shared by every mode

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

TEST_CASE("Move modes: a mode round-trips through its asset file as its own class; a missing key keeps its default")
{
    const TestTempDir     lDir("modeRoundTrip");
    const MovementClasses lClasses;

    GroundMoveMode lSaved;
    lSaved.MaxSpeed         = 375.f;
    lSaved.JumpSpeed        = 640.f;
    lSaved.MaxSlopeAngleDeg = 60.f;

    const OpaaxString lPath = lDir.Sub("Hero_Ground.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lSaved));

    // The base's field and the mode's, under the mode's class.
    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Load(lPath, lContents));
    CHECK(lContents.Class == OPAAX_ID("GroundMoveMode"));
    CHECK(lContents.Properties.contains("MaxSlopeAngleDeg"));
    CHECK(lContents.Properties.contains("JumpSpeed"));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lPath, lClasses);
    const GroundMoveMode* lMode = Cast<GroundMoveMode>(lLoaded.Get());
    REQUIRE(lMode != nullptr);
    CHECK(lMode->MaxSpeed == doctest::Approx(375.f));
    CHECK(lMode->JumpSpeed == doctest::Approx(640.f));
    CHECK(lMode->MaxSlopeAngleDeg == doctest::Approx(60.f));

    // Written before JumpSpeed existed: it still loads, with that field's default.
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString(R"({ "Class": "GroundMoveMode", "Properties": { "MaxSpeed": 900.0 } })")));

    const TStrongObjectPtr<Asset> lShort = AssetFile::LoadAsset(lPartial, lClasses);
    const GroundMoveMode* lShortMode = Cast<GroundMoveMode>(lShort.Get());
    REQUIRE(lShortMode != nullptr);
    CHECK(lShortMode->MaxSpeed == doctest::Approx(900.f));
    CHECK(lShortMode->JumpSpeed == doctest::Approx(GroundMoveMode{}.JumpSpeed));
}

TEST_CASE("Move modes: a file of the old MoveMode form loads as the mode its Mode named")
{
    const TestTempDir     lDir("modeOldForm");
    const MovementClasses lClasses;

    // Before the modes were classes: one MoveMode class, the mode named by a field, every field saved.
    const OpaaxString lPath = lDir.Sub("Hero_Fly.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lPath, OpaaxString(R"({ "Class": "MoveMode", "Properties": {
        "Mode": "FlyMove", "MaxSpeed": 650.0, "JumpSpeed": 500.0, "MaxSlopeAngleDeg": 40.0 }, "Version": 1 })")));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lPath, lClasses);
    const FlyMoveMode* lFly = Cast<FlyMoveMode>(lLoaded.Get());
    REQUIRE(lFly != nullptr);
    CHECK(lFly->MaxSpeed == doctest::Approx(650.f));
    CHECK(lFly->MaxSlopeAngleDeg == doctest::Approx(40.f));

    // The file itself is untouched until --upgrade-assets rewrites it.
    AssetFile::Contents lRaw;
    REQUIRE(AssetFile::Parse(FileIO::ReadAllText(lPath), lRaw));
    CHECK(lRaw.Class == OPAAX_ID("MoveMode"));

    // The base cannot be made by itself: a file naming it without a mode does not load.
    const OpaaxString lBare = lDir.Sub("Bare.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lBare, OpaaxString(R"({ "Class": "MoveMode", "Properties": { "MaxSpeed": 1.0 } })")));
    CHECK_FALSE(AssetFile::LoadAsset(lBare, lClasses).IsValid());
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
