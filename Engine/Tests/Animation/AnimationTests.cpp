// Suite: the animation assets — AnimationClip (SampleClip, stateless playback) and AnimationLibrary
// (clips under short names), through their asset files and the conversion of the old .opaaxclip and
// .opaaxanim formats. Timing cases use Fps = 1, so a second is a tick.
// File cases use a unique temp directory, created and removed per case.
#include <doctest.h>

#include "../LegacyAssetFile.h"
#include "../TestTempDir.h"

#include "Animation/Assets/AnimationClip.h"
#include "Animation/Assets/AnimationLibrary.h"
#include "Core/IO/FileIO.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"

using namespace Opaax;

namespace
{
    struct AnimationClasses final : ObjectClassRegistry
    {
        AnimationClasses()
        {
            Register<AnimationClip>();
            Register<AnimationLibrary>();
        }
    };

    /** Makes OutClip a sheet-based clip of InStepCount steps, one tick each, at 1 fps. */
    void FillProbeClip(AnimationClip& OutClip, const Uint32 InStepCount, const EAnimPlayMode InMode)
    {
        OutClip.Sheet.Path = OpaaxString("Sheets/Hero.opaaxsheet");
        OutClip.Fps        = 1.f;
        OutClip.PlayMode   = InMode;
        OutClip.Steps.clear();

        for (Uint32 lIndex = 0u; lIndex < InStepCount; ++lIndex)
        {
            OutClip.Steps.emplace_back(AnimationStep{ OpaaxStringID(), {}, 1u });
        }
    }

    AnimationLibraryEntry MakeEntry(const char* InName, const char* InClipPath)
    {
        AnimationLibraryEntry lEntry;
        lEntry.Name      = (InName[0] == '\0') ? OpaaxStringID() : OpaaxStringID(InName);
        lEntry.Clip.Path = OpaaxString(InClipPath);
        return lEntry;
    }

    /** Makes OutLibrary Idle / Run / Jump, with Run as the default. */
    void FillProbeLibrary(AnimationLibrary& OutLibrary)
    {
        OutLibrary.DefaultClip = OPAAX_ID("Run");
        OutLibrary.Entries     = { MakeEntry("Idle", "Anims/Idle.opaaxasset"),
                                   MakeEntry("Run", "Anims/Run.opaaxasset"),
                                   MakeEntry("Jump", "Anims/Jump.opaaxasset") };
    }
}

// =============================================================================
// SampleClip — the playback model
// =============================================================================
TEST_CASE("SampleClip: a Loop clip walks its steps and wraps, forever")
{
    AnimationClip lClip;
    FillProbeClip(lClip, 4u, EAnimPlayMode::Loop);

    REQUIRE(lClip.TotalTicks() == 4u);

    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK(SampleClip(lClip, 1.f).Step == 1u);
    CHECK(SampleClip(lClip, 3.f).Step == 3u);

    // The wrap, which is the whole point of the mode.
    CHECK(SampleClip(lClip, 4.f).Step == 0u);
    CHECK(SampleClip(lClip, 5.f).Step == 1u);
    CHECK(SampleClip(lClip, 401.f).Step == 1u);

    // A looping clip that can advance NEVER finishes — a caller that stopped accumulating on it
    // would freeze the animation.
    CHECK_FALSE(SampleClip(lClip, 0.f).bFinished);
    CHECK_FALSE(SampleClip(lClip, 4.f).bFinished);
    CHECK_FALSE(SampleClip(lClip, 401.f).bFinished);
}

TEST_CASE("SampleClip: mid-tick time resolves to the tick it is inside")
{
    // The floor, asserted rather than assumed: a step must hold for its whole tick, not swap at
    // the halfway point.
    AnimationClip lClip;
    FillProbeClip(lClip, 4u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lClip, 0.99f).Step == 0u);
    CHECK(SampleClip(lClip, 1.01f).Step == 1u);
    CHECK(SampleClip(lClip, 3.99f).Step == 3u);
}

TEST_CASE("SampleClip: a Once clip stops on its LAST step and says it finished")
{
    AnimationClip lClip;
    FillProbeClip(lClip, 3u, EAnimPlayMode::Once);

    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK_FALSE(SampleClip(lClip, 0.f).bFinished);
    CHECK(SampleClip(lClip, 2.f).Step == 2u);

    // The last tick is still PLAYING — finished means the clip has run out, not that it is on the
    // final step.
    CHECK_FALSE(SampleClip(lClip, 2.f).bFinished);

    // Past the end it HOLDS the last step rather than wrapping or blanking.
    CHECK(SampleClip(lClip, 3.f).Step == 2u);
    CHECK(SampleClip(lClip, 900.f).Step == 2u);
    CHECK(SampleClip(lClip, 3.f).bFinished);
    CHECK(SampleClip(lClip, 900.f).bFinished);
}

TEST_CASE("SampleClip: a PingPong clip walks back without holding either end twice")
{
    // 4 steps walk 0 1 2 3 2 1 and repeat — a period of 6, not 8. Holding the ends twice is the
    // plausible-looking wrong answer here, so the turn-around ticks are asserted individually.
    AnimationClip lClip;
    FillProbeClip(lClip, 4u, EAnimPlayMode::PingPong);

    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK(SampleClip(lClip, 1.f).Step == 1u);
    CHECK(SampleClip(lClip, 2.f).Step == 2u);
    CHECK(SampleClip(lClip, 3.f).Step == 3u);
    CHECK(SampleClip(lClip, 4.f).Step == 2u);
    CHECK(SampleClip(lClip, 5.f).Step == 1u);
    CHECK(SampleClip(lClip, 6.f).Step == 0u);
    CHECK(SampleClip(lClip, 7.f).Step == 1u);

    CHECK_FALSE(SampleClip(lClip, 4.f).bFinished);
}

TEST_CASE("SampleClip: a two-step PingPong is 0 1 0 1, and a one-step one never moves")
{
    // The period arithmetic (2*total - 2) degenerates at 1 and 2 steps; both are real clips.
    AnimationClip lTwo;
    FillProbeClip(lTwo, 2u, EAnimPlayMode::PingPong);

    CHECK(SampleClip(lTwo, 0.f).Step == 0u);
    CHECK(SampleClip(lTwo, 1.f).Step == 1u);
    CHECK(SampleClip(lTwo, 2.f).Step == 0u);
    CHECK(SampleClip(lTwo, 3.f).Step == 1u);

    AnimationClip lOne;
    FillProbeClip(lOne, 1u, EAnimPlayMode::PingPong);

    CHECK(SampleClip(lOne, 0.f).Step == 0u);
    CHECK(SampleClip(lOne, 1.f).Step == 0u);
    CHECK(SampleClip(lOne, 99.f).Step == 0u);
}

TEST_CASE("SampleClip: Hold stretches a step, and the total is the SUM of the holds")
{
    AnimationClip lClip;
    FillProbeClip(lClip, 2u, EAnimPlayMode::Loop);
    lClip.Steps[0].Hold = 3u;   // 3 ticks on step 0, 1 on step 1

    REQUIRE(lClip.TotalTicks() == 4u);

    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK(SampleClip(lClip, 1.f).Step == 0u);
    CHECK(SampleClip(lClip, 2.f).Step == 0u);
    CHECK(SampleClip(lClip, 3.f).Step == 1u);
    CHECK(SampleClip(lClip, 4.f).Step == 0u);   // wrapped
}

TEST_CASE("SampleClip: a Hold of ZERO counts as one — a step never silently vanishes")
{
    // Dropping a mistyped 0-hold step would shift every index after it, which is the silent
    // wrong-answer failure. It holds for one tick instead.
    AnimationClip lClip;
    FillProbeClip(lClip, 2u, EAnimPlayMode::Loop);
    lClip.Steps[0].Hold = 0u;

    CHECK(lClip.Steps[0].EffectiveHold() == 1u);
    CHECK(lClip.TotalTicks() == 2u);
    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK(SampleClip(lClip, 1.f).Step == 1u);
}

TEST_CASE("SampleClip: Fps scales time, so the same clip runs at two speeds")
{
    AnimationClip lClip;
    FillProbeClip(lClip, 4u, EAnimPlayMode::Loop);
    lClip.Fps = 10.f;

    CHECK(SampleClip(lClip, 0.05f).Step == 0u);
    CHECK(SampleClip(lClip, 0.10f).Step == 1u);
    CHECK(SampleClip(lClip, 0.30f).Step == 3u);
    CHECK(SampleClip(lClip, 0.40f).Step == 0u);
}

TEST_CASE("SampleClip: a clip that cannot advance answers step 0 and reports FINISHED")
{
    // No steps, a zero Fps and a negative Fps all mean "nothing new will ever be shown". Answering
    // finished is what stops a caller accumulating time forever, and step 0 is always in range.
    AnimationClip lEmpty;
    FillProbeClip(lEmpty, 0u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lEmpty, 0.f).Step == 0u);
    CHECK(SampleClip(lEmpty, 5.f).Step == 0u);
    CHECK(SampleClip(lEmpty, 5.f).bFinished);

    AnimationClip lZeroFps;
    FillProbeClip(lZeroFps, 4u, EAnimPlayMode::Loop);
    lZeroFps.Fps = 0.f;

    CHECK(SampleClip(lZeroFps, 5.f).Step == 0u);
    CHECK(SampleClip(lZeroFps, 5.f).bFinished);

    AnimationClip lNegativeFps;
    FillProbeClip(lNegativeFps, 4u, EAnimPlayMode::Loop);
    lNegativeFps.Fps = -12.f;

    CHECK(SampleClip(lNegativeFps, 5.f).Step == 0u);
    CHECK(SampleClip(lNegativeFps, 5.f).bFinished);
}

TEST_CASE("SampleClip: negative time answers the first step rather than an out-of-range index")
{
    // Cannot happen from a forward-running clock; the function is total anyway, because the one
    // thing it must never do is index past the steps.
    AnimationClip lClip;
    FillProbeClip(lClip, 4u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lClip, -1.f).Step == 0u);
    CHECK(SampleClip(lClip, -900.f).Step == 0u);
}

// =============================================================================
// AnimationClip::StepAt
// =============================================================================
TEST_CASE("StepAt: out of range answers NULL rather than clamping")
{
    AnimationClip lClip;
    FillProbeClip(lClip, 2u, EAnimPlayMode::Loop);
    lClip.Steps[1].Frame = OPAAX_ID("Hero_Idle_1");

    REQUIRE(lClip.StepAt(1) != nullptr);
    CHECK(lClip.StepAt(1)->Frame == OPAAX_ID("Hero_Idle_1"));
    CHECK(lClip.StepAt(2) == nullptr);
    CHECK(lClip.StepAt(999) == nullptr);

    const AnimationClip lEmpty;
    CHECK(lEmpty.StepAt(0) == nullptr);
}

// =============================================================================
// The clip's file
// =============================================================================
TEST_CASE("AnimationClip: save then load round-trips every field")
{
    const TestTempDir      lDir("clipRoundTrip");
    const AnimationClasses lClasses;
    const OpaaxString      lPath = lDir.Sub("Hero_Idle.opaaxasset");

    AnimationClip lSaved;
    FillProbeClip(lSaved, 3u, EAnimPlayMode::PingPong);
    lSaved.Fps                   = 8.f;
    lSaved.Steps[0].Frame        = OPAAX_ID("Hero_Idle_0");
    lSaved.Steps[1].Hold         = 4u;
    lSaved.Steps[2].Texture.Path = OpaaxString("Textures/Blink.png");

    REQUIRE(AssetFile::Save(lPath, lSaved));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
    const AnimationClip*          lLoaded = Cast<AnimationClip>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);

    CHECK(lLoaded->Sheet.Path == lSaved.Sheet.Path);
    CHECK(lLoaded->Fps == doctest::Approx(8.f));
    CHECK(lLoaded->StepCount() == 3u);
    CHECK(lLoaded->TotalTicks() == 6u);

    // The enum crosses as its LABEL, so reordering the enumerators cannot change what a file means.
    CHECK(lLoaded->PlayMode == EAnimPlayMode::PingPong);
    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Load(lPath, lContents));
    CHECK(lContents.Properties["PlayMode"] == "PingPong");

    // The name survives as an ID that COMPARES EQUAL — the interned round trip, not just the text.
    CHECK(lLoaded->Steps[0].Frame == OPAAX_ID("Hero_Idle_0"));
    CHECK_FALSE(lLoaded->Steps[1].Frame.IsValid());
    CHECK(lLoaded->Steps[1].Hold == 4u);
    CHECK(lLoaded->Steps[2].Texture.Path == OpaaxString("Textures/Blink.png"));

    // Saving what was loaded writes the same text: the editor's dirty check compares it.
    CHECK(AssetFile::Serialize(*lLoaded) == AssetFile::Serialize(lSaved));
}

TEST_CASE("AnimationClip: a file missing keys keeps the defaults; a wrong type or play mode is refused")
{
    const TestTempDir      lDir("clipPartial");
    const AnimationClasses lClasses;

    // Adding a field must not break clips saved before it.
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString(R"({ "Class": "AnimationClip", "Properties": { "Fps": 24.0 } })")));

    const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPartial, lClasses);
    const AnimationClip*          lLoaded = Cast<AnimationClip>(lAsset.Get());
    REQUIRE(lLoaded != nullptr);
    CHECK(lLoaded->Fps == doctest::Approx(24.f));
    CHECK(lLoaded->Steps.empty());
    CHECK(lLoaded->PlayMode == EAnimPlayMode::Loop);   // the class's own default
    CHECK(lLoaded->Sheet.Path.IsEmpty());

    const OpaaxString lWrongType = lDir.Sub("WrongType.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lWrongType, OpaaxString(R"({ "Class": "AnimationClip", "Properties": { "Fps": "fast" } })")));
    CHECK(AssetFile::LoadAsset(lWrongType, lClasses).Get() == nullptr);

    // A misspelled enumerator is refused rather than silently corrected to a default — the editor's
    // dropdown cannot author one, so a bad label means a hand edit that deserves to be told.
    const OpaaxString lBadMode = lDir.Sub("BadMode.opaaxasset");
    REQUIRE(FileIO::WriteAllText(lBadMode, OpaaxString(R"({ "Class": "AnimationClip", "Properties": { "PlayMode": "Bounce" } })")));
    CHECK(AssetFile::LoadAsset(lBadMode, lClasses).Get() == nullptr);
}

// =============================================================================
// AnimationLibrary — the alias table, and the one asymmetry in its lookup
// =============================================================================
TEST_SUITE("AnimationLibrary")
{
    TEST_CASE("Find: a name resolves to its own entry")
    {
        AnimationLibrary lLibrary;
        FillProbeLibrary(lLibrary);

        REQUIRE(lLibrary.Find(OPAAX_ID("Jump")) != nullptr);
        CHECK(lLibrary.Find(OPAAX_ID("Jump"))->Clip.Path == OpaaxString("Anims/Jump.opaaxasset"));
        CHECK(lLibrary.EntryCount() == 3u);
    }

    TEST_CASE("Find: NO OPINION falls back, a MISSPELLING does not")
    {
        // The one asymmetry in this type, and the reason it is asserted rather than assumed:
        // playing some other animation because a name was mistyped is the silent wrong answer.
        AnimationLibrary lLibrary;
        FillProbeLibrary(lLibrary);

        REQUIRE(lLibrary.Find(OpaaxStringID()) != nullptr);
        CHECK(lLibrary.Find(OpaaxStringID())->Name == OPAAX_ID("Run"));   // the DefaultClip

        CHECK(lLibrary.Find(OPAAX_ID("Runn")) == nullptr);
        CHECK(lLibrary.Find(OPAAX_ID("Attack")) == nullptr);
    }

    TEST_CASE("Find: no opinion and no usable default answers the FIRST entry")
    {
        AnimationLibrary lNoDefault;
        FillProbeLibrary(lNoDefault);
        lNoDefault.DefaultClip = OpaaxStringID();

        REQUIRE(lNoDefault.Find(OpaaxStringID()) != nullptr);
        CHECK(lNoDefault.Find(OpaaxStringID())->Name == OPAAX_ID("Idle"));

        // A default naming a clip that was since removed must not strand the lookup either.
        AnimationLibrary lStaleDefault;
        FillProbeLibrary(lStaleDefault);
        lStaleDefault.DefaultClip = OPAAX_ID("Deleted");

        REQUIRE(lStaleDefault.Find(OpaaxStringID()) != nullptr);
        CHECK(lStaleDefault.Find(OpaaxStringID())->Name == OPAAX_ID("Idle"));
    }

    TEST_CASE("Find and FindExact: an empty library resolves nothing, either way")
    {
        const AnimationLibrary lEmpty;

        CHECK(lEmpty.Find(OpaaxStringID()) == nullptr);
        CHECK(lEmpty.Find(OPAAX_ID("Idle")) == nullptr);
        CHECK(lEmpty.FindExact(OPAAX_ID("Idle")) == nullptr);
    }

    TEST_CASE("FindExact: no fallback at all, which is what a rename check needs")
    {
        AnimationLibrary lLibrary;
        FillProbeLibrary(lLibrary);

        CHECK(lLibrary.FindExact(OPAAX_ID("Idle")) != nullptr);
        CHECK(lLibrary.FindExact(OpaaxStringID()) == nullptr);   // NOT the default
        CHECK(lLibrary.FindExact(OPAAX_ID("Nope")) == nullptr);
    }

    TEST_CASE("An edit keeps the names usable")
    {
        AnimationLibrary lLibrary;
        lLibrary.Entries = {
            MakeEntry("Run", "Anims/Hero_Run.opaaxasset"),
            MakeEntry("Run", "Anims/Hero_Sprint.opaaxasset"),   // a repeated name: could never be reached
            MakeEntry("", "Anims/Hero_Jump.opaaxasset"),        // a dropped clip, no name yet
            MakeEntry("", "Other/Hero_Jump.opaaxasset"),        // its file's name is already taken
        };
        lLibrary.DefaultClip = OPAAX_ID("Walk");                 // names no entry

        lLibrary.PostEditChange();

        CHECK(lLibrary.Entries[0].Name == OPAAX_ID("Run"));
        CHECK(lLibrary.Entries[1].Name == OPAAX_ID("Hero_Sprint"));   // cleared, then named after its file
        CHECK(lLibrary.Entries[2].Name == OPAAX_ID("Hero_Jump"));
        CHECK_FALSE(lLibrary.Entries[3].Name.IsValid());
        CHECK_FALSE(lLibrary.DefaultClip.IsValid());

        // A valid default stays.
        lLibrary.DefaultClip = OPAAX_ID("Hero_Jump");
        lLibrary.PostEditChange();
        CHECK(lLibrary.DefaultClip == OPAAX_ID("Hero_Jump"));
    }

    TEST_CASE("Save then load round-trips the names, their order and the default")
    {
        const TestTempDir      lDir("libRoundTrip");
        const AnimationClasses lClasses;
        const OpaaxString      lPath = lDir.Sub("Hero.opaaxasset");

        AnimationLibrary lSaved;
        FillProbeLibrary(lSaved);
        REQUIRE(AssetFile::Save(lPath, lSaved));

        const TStrongObjectPtr<Asset> lAsset  = AssetFile::LoadAsset(lPath, lClasses);
        const AnimationLibrary*       lLoaded = Cast<AnimationLibrary>(lAsset.Get());
        REQUIRE(lLoaded != nullptr);

        CHECK(lLoaded->EntryCount() == 3u);
        CHECK(lLoaded->DefaultClip == OPAAX_ID("Run"));
        REQUIRE(lLoaded->Find(OPAAX_ID("Idle")) != nullptr);
        CHECK(lLoaded->Find(OPAAX_ID("Idle"))->Clip.Path == OpaaxString("Anims/Idle.opaaxasset"));

        // Order is kept, because "the first entry" is a documented fallback.
        CHECK(lLoaded->Entries[0].Name == OPAAX_ID("Idle"));
        CHECK(AssetFile::Serialize(*lLoaded) == AssetFile::Serialize(lSaved));
    }
}

// =============================================================================
// The old formats
// =============================================================================
TEST_CASE("AnimationAssets: the old .opaaxclip and .opaaxanim files convert with their values")
{
    const TestTempDir      lDir("animConvert");
    const AnimationClasses lClasses;

    const TStrongObjectPtr<Asset> lClipAsset = ConvertAndLoad(lDir, "Anims/Hero_Run.opaaxclip", R"({
        "Sheet": "Sheets/Hero.opaaxsheet", "Fps": 10.0, "PlayMode": "PingPong",
        "Steps": [ { "Frame": "Run_0", "Hold": 2 }, { "Frame": "Run_1" } ] })", lClasses);

    const AnimationClip* lClip = Cast<AnimationClip>(lClipAsset.Get());
    REQUIRE(lClip != nullptr);
    CHECK(lClip->Sheet.Path == "Sheets/Hero.opaaxsheet");
    CHECK(lClip->Fps == doctest::Approx(10.f));
    CHECK(lClip->PlayMode == EAnimPlayMode::PingPong);
    REQUIRE(lClip->StepCount() == 2u);
    CHECK(lClip->Steps[0].Frame == OPAAX_ID("Run_0"));
    CHECK(lClip->Steps[0].Hold == 2u);
    CHECK(lClip->Steps[1].Hold == 1u);   // missing: the default

    const TStrongObjectPtr<Asset> lLibraryAsset = ConvertAndLoad(lDir, "Anims/Hero.opaaxanim", R"({
        "DefaultClip": "Run",
        "Entries": [ { "Name": "Idle", "Clip": "Anims/Hero_Idle.opaaxasset" },
                     { "Name": "Run",  "Clip": "Anims/Hero_Run.opaaxasset" } ] })", lClasses);

    const AnimationLibrary* lLibrary = Cast<AnimationLibrary>(lLibraryAsset.Get());
    REQUIRE(lLibrary != nullptr);
    CHECK(lLibrary->DefaultClip == OPAAX_ID("Run"));
    REQUIRE(lLibrary->Find(OpaaxStringID()) != nullptr);
    CHECK(lLibrary->Find(OpaaxStringID())->Clip.Path == "Anims/Hero_Run.opaaxasset");
}
