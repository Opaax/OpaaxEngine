// Suite: the animation CLIP asset — its data, its `.opaaxclip` file, and the pure function the
// whole feature stands on (SampleClip).
//
// SampleClip is free and pure precisely so it can be tested with no world, no GL context and no
// clock, the way MakeFrameUV, SliceGrid and PlanQuadBatches are. It earns it twice over: it is
// STATELESS (the answer is a function of the time, never of the previous frame), so these cases
// pin the whole playback model rather than one transition.
//
// Every timing case uses Fps = 1, so a second IS a tick and the expected values can be read off
// the clip by eye. The file cases run against a unique temp directory, created and removed per
// case — never the repo's own assets ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <string>

#include "Core/IO/FileIO.h"   // the malformed-file cases author their own bytes
#include "Engine/Subsystems/Resources/ResourceManager.h"        // completes LoadContext
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipData.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipFile.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipResource.h"
#include "Engine/Subsystems/Resources/Types/AnimationLibraryData.h"
#include "Engine/Subsystems/Resources/Types/AnimationLibraryFile.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxAnimTests_" + std::string(InTag));

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

    /** A sheet-based clip of InStepCount steps, one tick each, at 1 fps. */
    AnimationClipData MakeProbeClip(const Uint32 InStepCount, const EAnimPlayMode InMode)
    {
        AnimationClipData lClip;
        lClip.Sheet.Path = OpaaxString("Sheets/Hero.opaaxsheet");
        lClip.Fps        = 1.f;
        lClip.PlayMode   = InMode;

        for (Uint32 lIndex = 0u; lIndex < InStepCount; ++lIndex)
        {
            lClip.Steps.emplace_back(AnimationStep{ OpaaxStringID(), {}, 1u });
        }

        return lClip;
    }
}

// =============================================================================
// SampleClip — the playback model
// =============================================================================
TEST_CASE("SampleClip: a Loop clip walks its steps and wraps, forever")
{
    const AnimationClipData lClip = MakeProbeClip(4u, EAnimPlayMode::Loop);

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
    const AnimationClipData lClip = MakeProbeClip(4u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lClip, 0.99f).Step == 0u);
    CHECK(SampleClip(lClip, 1.01f).Step == 1u);
    CHECK(SampleClip(lClip, 3.99f).Step == 3u);
}

TEST_CASE("SampleClip: a Once clip stops on its LAST step and says it finished")
{
    const AnimationClipData lClip = MakeProbeClip(3u, EAnimPlayMode::Once);

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
    const AnimationClipData lClip = MakeProbeClip(4u, EAnimPlayMode::PingPong);

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
    const AnimationClipData lTwo = MakeProbeClip(2u, EAnimPlayMode::PingPong);

    CHECK(SampleClip(lTwo, 0.f).Step == 0u);
    CHECK(SampleClip(lTwo, 1.f).Step == 1u);
    CHECK(SampleClip(lTwo, 2.f).Step == 0u);
    CHECK(SampleClip(lTwo, 3.f).Step == 1u);

    const AnimationClipData lOne = MakeProbeClip(1u, EAnimPlayMode::PingPong);

    CHECK(SampleClip(lOne, 0.f).Step == 0u);
    CHECK(SampleClip(lOne, 1.f).Step == 0u);
    CHECK(SampleClip(lOne, 99.f).Step == 0u);
}

TEST_CASE("SampleClip: Hold stretches a step, and the total is the SUM of the holds")
{
    AnimationClipData lClip = MakeProbeClip(2u, EAnimPlayMode::Loop);
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
    AnimationClipData lClip = MakeProbeClip(2u, EAnimPlayMode::Loop);
    lClip.Steps[0].Hold = 0u;

    CHECK(lClip.Steps[0].EffectiveHold() == 1u);
    CHECK(lClip.TotalTicks() == 2u);
    CHECK(SampleClip(lClip, 0.f).Step == 0u);
    CHECK(SampleClip(lClip, 1.f).Step == 1u);
}

TEST_CASE("SampleClip: Fps scales time, so the same clip runs at two speeds")
{
    AnimationClipData lClip = MakeProbeClip(4u, EAnimPlayMode::Loop);
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
    const AnimationClipData lEmpty = MakeProbeClip(0u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lEmpty, 0.f).Step == 0u);
    CHECK(SampleClip(lEmpty, 5.f).Step == 0u);
    CHECK(SampleClip(lEmpty, 5.f).bFinished);

    AnimationClipData lZeroFps = MakeProbeClip(4u, EAnimPlayMode::Loop);
    lZeroFps.Fps = 0.f;

    CHECK(SampleClip(lZeroFps, 5.f).Step == 0u);
    CHECK(SampleClip(lZeroFps, 5.f).bFinished);

    AnimationClipData lNegativeFps = MakeProbeClip(4u, EAnimPlayMode::Loop);
    lNegativeFps.Fps = -12.f;

    CHECK(SampleClip(lNegativeFps, 5.f).Step == 0u);
    CHECK(SampleClip(lNegativeFps, 5.f).bFinished);
}

TEST_CASE("SampleClip: negative time answers the first step rather than an out-of-range index")
{
    // Cannot happen from a forward-running clock; the function is total anyway, because the one
    // thing it must never do is index past the steps.
    const AnimationClipData lClip = MakeProbeClip(4u, EAnimPlayMode::Loop);

    CHECK(SampleClip(lClip, -1.f).Step == 0u);
    CHECK(SampleClip(lClip, -900.f).Step == 0u);
}

// =============================================================================
// AnimationClipData::StepAt
// =============================================================================
TEST_CASE("StepAt: out of range answers NULL rather than clamping")
{
    AnimationClipData lClip = MakeProbeClip(2u, EAnimPlayMode::Loop);
    lClip.Steps[1].Frame = OPAAX_ID("Hero_Idle_1");

    REQUIRE(lClip.StepAt(1) != nullptr);
    CHECK(lClip.StepAt(1)->Frame == OPAAX_ID("Hero_Idle_1"));
    CHECK(lClip.StepAt(2) == nullptr);
    CHECK(lClip.StepAt(999) == nullptr);

    const AnimationClipData lEmpty;
    CHECK(lEmpty.StepAt(0) == nullptr);
}

// =============================================================================
// The file
// =============================================================================
TEST_CASE("AnimationClipFile: save then load round-trips every field")
{
    const ScopedTempDir lDir("roundtrip");
    const OpaaxString   lPath = lDir.Sub("Hero_Idle.opaaxclip");

    AnimationClipData lSaved = MakeProbeClip(3u, EAnimPlayMode::PingPong);
    lSaved.Fps                = 8.f;
    lSaved.Steps[0].Frame     = OPAAX_ID("Hero_Idle_0");
    lSaved.Steps[1].Hold      = 4u;
    lSaved.Steps[2].Texture.Path = OpaaxString("Textures/Blink.png");

    REQUIRE(AnimationClipFile::Save(lPath, lSaved));

    AnimationClipData lLoaded;
    REQUIRE(AnimationClipFile::Load(lPath, lLoaded));

    CHECK(lLoaded.Sheet.Path == lSaved.Sheet.Path);
    CHECK(lLoaded.Fps == doctest::Approx(8.f));
    CHECK(lLoaded.StepCount() == 3u);
    CHECK(lLoaded.TotalTicks() == 6u);

    // The enum crosses as its LABEL, so reordering the enumerators cannot change what a file means.
    CHECK(lLoaded.PlayMode == EAnimPlayMode::PingPong);

    // The name survives as an ID that COMPARES EQUAL — the interned round trip, not just the text.
    CHECK(lLoaded.Steps[0].Frame == OPAAX_ID("Hero_Idle_0"));
    CHECK_FALSE(lLoaded.Steps[1].Frame.IsValid());
    CHECK(lLoaded.Steps[1].Hold == 4u);
    CHECK(lLoaded.Steps[2].Texture.Path == OpaaxString("Textures/Blink.png"));
}

TEST_CASE("AnimationClipFile: the text a save writes is the text a save writes again")
{
    // What the editor's dirty marker stands on: Serialize must be stable, or a `*` appears on a
    // clip nobody touched (the trap L30 records for the map baseline).
    const AnimationClipData lData = MakeProbeClip(3u, EAnimPlayMode::Once);

    CHECK(AnimationClipFile::Serialize(lData) == AnimationClipFile::Serialize(lData));

    const ScopedTempDir lDir("stable");
    const OpaaxString   lPath = lDir.Sub("Stable.opaaxclip");
    REQUIRE(AnimationClipFile::Save(lPath, lData));

    AnimationClipData lLoaded;
    REQUIRE(AnimationClipFile::Load(lPath, lLoaded));

    CHECK(AnimationClipFile::Serialize(lLoaded) == AnimationClipFile::Serialize(lData));
}

TEST_CASE("AnimationClipFile: a missing or malformed file leaves the caller's data untouched")
{
    const ScopedTempDir lDir("bad");

    AnimationClipData lExisting = MakeProbeClip(3u, EAnimPlayMode::Loop);

    CHECK_FALSE(AnimationClipFile::Load(lDir.Sub("NoSuchFile.opaaxclip"), lExisting));
    CHECK(lExisting.StepCount() == 3u);   // not half-overwritten

    // Valid json, wrong shape: an array is not a clip.
    const OpaaxString lArrayPath = lDir.Sub("Array.opaaxclip");
    REQUIRE(FileIO::WriteAllText(lArrayPath, OpaaxString("[1, 2, 3]")));

    CHECK_FALSE(AnimationClipFile::Load(lArrayPath, lExisting));
    CHECK(lExisting.StepCount() == 3u);
}

TEST_CASE("AnimationClipFile: a file missing keys keeps the defaults (_WITH_DEFAULT)")
{
    const ScopedTempDir lDir("partial");

    // Adding a field must never refuse a clip written before it existed — the same rule that keeps
    // every .opaaxmap loading (I8). This is the case that will matter when the notify track lands.
    const OpaaxString lPartial = lDir.Sub("Partial.opaaxclip");
    REQUIRE(FileIO::WriteAllText(lPartial, OpaaxString("{\n    \"Fps\": 24.0\n}")));

    AnimationClipData lLoaded;
    REQUIRE(AnimationClipFile::Load(lPartial, lLoaded));
    CHECK(lLoaded.Fps == doctest::Approx(24.f));
    CHECK(lLoaded.Steps.empty());
    CHECK(lLoaded.PlayMode == EAnimPlayMode::Loop);   // the struct's own default
    CHECK(lLoaded.Sheet.Path.IsEmpty());
}

TEST_CASE("AnimationClipFile: a wrong TYPE and an unknown PlayMode label are both refused")
{
    const ScopedTempDir lDir("wrong");

    const OpaaxString lWrongType = lDir.Sub("WrongType.opaaxclip");
    REQUIRE(FileIO::WriteAllText(lWrongType, OpaaxString("{\n    \"Fps\": \"fast\"\n}")));

    AnimationClipData lUntouched;
    CHECK_FALSE(AnimationClipFile::Load(lWrongType, lUntouched));

    // A misspelled enumerator THROWS rather than silently correcting to a default — the editor's
    // dropdown cannot author one, so a bad label means a hand edit that deserves to be told.
    const OpaaxString lBadMode = lDir.Sub("BadMode.opaaxclip");
    REQUIRE(FileIO::WriteAllText(lBadMode, OpaaxString("{\n    \"PlayMode\": \"Bounce\"\n}")));

    CHECK_FALSE(AnimationClipFile::Load(lBadMode, lUntouched));
}

// =============================================================================
// AnimationClipResource — the CResource adapter, through the REAL manager
//
//   The file cases above prove the FORMAT. These prove the RESOURCE: that the adapter refuses a
//   bad file rather than half-loading it, and that its Placeholder policy resolves to something
//   the animator can safely sample. Without them the whole `.opaaxclip` -> ResourceManager route
//   would be verified only by the fact that it compiles.
// =============================================================================
TEST_SUITE("AnimationClipResource")
{
    /** A context to hand Load. Every case is a leaf load — a clip acquires no child (SS3). */
    struct LoadFixture
    {
        ResourceManager         Manager;
        ResourceDependencyGraph Deps;
        LoadContext             Ctx{ Manager, Deps };
    };

    TEST_CASE("loads a real .opaaxclip off disk")
    {
        const ScopedTempDir lDir("res_load");
        const OpaaxString   lPath = lDir.Sub("Hero_Run.opaaxclip");

        AnimationClipData lAuthored = MakeProbeClip(4u, EAnimPlayMode::Loop);
        lAuthored.Fps = 16.f;
        REQUIRE(AnimationClipFile::Save(lPath, lAuthored));

        LoadFixture lFixture;
        std::optional<AnimationClipResource> lClip = AnimationClipResource::Load(lPath.CStr(), lFixture.Ctx);

        REQUIRE(lClip.has_value());
        CHECK(lClip->Data.StepCount() == 4u);
        CHECK(lClip->Data.Fps == doctest::Approx(16.f));
        CHECK(lClip->Data.Sheet.Path == OpaaxString("Sheets/Hero.opaaxsheet"));

        // Structural, so it answers the same before and after anything reads the payload — the
        // property the pool's accounting rests on.
        CHECK(lClip->ByteSize() > sizeof(AnimationClipResource));
    }

    TEST_CASE("a missing or malformed clip is REFUSED, not half-loaded")
    {
        const ScopedTempDir lDir("res_bad");

        LoadFixture lFixture;

        CHECK_FALSE(AnimationClipResource::Load(lDir.Sub("Nope.opaaxclip").CStr(), lFixture.Ctx).has_value());

        const OpaaxString lGarbage = lDir.Sub("Garbage.opaaxclip");
        REQUIRE(FileIO::WriteAllText(lGarbage, OpaaxString("not json at all")));

        CHECK_FALSE(AnimationClipResource::Load(lGarbage.CStr(), lFixture.Ctx).has_value());
    }

    TEST_CASE("the placeholder is an EMPTY clip that samples as finished")
    {
        // The Placeholder policy's whole claim: a missing clip must leave a sprite showing its
        // AUTHORED frame, so the substitute has to be a clip the animator can sample and get
        // "nothing to show" from — never a null it would have to branch on.
        const AnimationClipResource lPlaceholder = AnimationClipResource::Placeholder();

        CHECK(lPlaceholder.Data.StepCount() == 0u);
        CHECK(lPlaceholder.Data.Sheet.Path.IsEmpty());
        CHECK(SampleClip(lPlaceholder.Data, 0.f).bFinished);
        CHECK(SampleClip(lPlaceholder.Data, 99.f).Step == 0u);
    }

    TEST_CASE("a broken clip RESOLVES to the placeholder through the manager, never to null")
    {
        // Placeholder vs FailFast is the difference that matters here, and it is only observable
        // through the real manager: a failed claim reports IsValid() false while Get() still
        // answers a payload. Gate on IsValid(), never on Get() != nullptr (I16).
        const ScopedTempDir lDir("res_degrade");
        const OpaaxString   lPath = lDir.Sub("Broken.opaaxclip");
        REQUIRE(FileIO::WriteAllText(lPath, OpaaxString("[]")));

        ResourceManager                    lResources;
        ResourceRef<AnimationClipResource> lRef = lResources.Load<AnimationClipResource>(lPath.CStr());

        CHECK_FALSE(lRef.IsValid());   // the load itself failed, and says so

        AnimationClipResource* lResolved = lRef.Get();
        REQUIRE(lResolved != nullptr);
        CHECK(lResolved->Data.StepCount() == 0u);

        lResources.FlushAll();
    }

    TEST_CASE("the .opaaxclip extension resolves to this type, and nothing else claims it")
    {
        ResourceFormatRegistry lRegistry;
        REQUIRE(lRegistry.Register<AnimationClipResource>(OPAAX_ID("AnimationClip")));

        const ResourceFormatEntry* lClip =
            lRegistry.FindByExtension(NormalizeExtension(AnimationClipFile::CLIP_EXTENSION));

        REQUIRE(lClip != nullptr);
        CHECK(lClip->TypeId == ResourceTypeID::Get<AnimationClipResource>());

        // Case-insensitively, the way every other format is matched — Windows paths are.
        CHECK(lRegistry.FindByExtension(NormalizeExtension(".OPAAXCLIP")) == lClip);
    }
}

// =============================================================================
// AnimationLibraryData — the ALIAS table, and the one asymmetry in its lookup
// =============================================================================
TEST_SUITE("AnimationLibrary")
{
    /** Idle / Run / Jump, with Run as the default. */
    AnimationLibraryData MakeProbeLibrary()
    {
        AnimationLibraryData lLibrary;
        lLibrary.DefaultClip = OPAAX_ID("Run");

        lLibrary.Entries.emplace_back(AnimationLibraryEntry{ OPAAX_ID("Idle"), { OpaaxString("Anims/Idle.opaaxclip") } });
        lLibrary.Entries.emplace_back(AnimationLibraryEntry{ OPAAX_ID("Run"),  { OpaaxString("Anims/Run.opaaxclip") } });
        lLibrary.Entries.emplace_back(AnimationLibraryEntry{ OPAAX_ID("Jump"), { OpaaxString("Anims/Jump.opaaxclip") } });

        return lLibrary;
    }

    TEST_CASE("Find: a name resolves to its own entry")
    {
        const AnimationLibraryData lLibrary = MakeProbeLibrary();

        REQUIRE(lLibrary.Find(OPAAX_ID("Jump")) != nullptr);
        CHECK(lLibrary.Find(OPAAX_ID("Jump"))->Clip.Path == OpaaxString("Anims/Jump.opaaxclip"));
        CHECK(lLibrary.EntryCount() == 3u);
    }

    TEST_CASE("Find: NO OPINION falls back, a MISSPELLING does not")
    {
        // The one asymmetry in this type, and the reason it is asserted rather than assumed:
        // playing some other animation because a name was mistyped is the silent wrong answer.
        const AnimationLibraryData lLibrary = MakeProbeLibrary();

        REQUIRE(lLibrary.Find(OpaaxStringID()) != nullptr);
        CHECK(lLibrary.Find(OpaaxStringID())->Name == OPAAX_ID("Run"));   // the DefaultClip

        CHECK(lLibrary.Find(OPAAX_ID("Runn")) == nullptr);
        CHECK(lLibrary.Find(OPAAX_ID("Attack")) == nullptr);
    }

    TEST_CASE("Find: no opinion and no usable default answers the FIRST entry")
    {
        AnimationLibraryData lNoDefault = MakeProbeLibrary();
        lNoDefault.DefaultClip = OpaaxStringID();

        REQUIRE(lNoDefault.Find(OpaaxStringID()) != nullptr);
        CHECK(lNoDefault.Find(OpaaxStringID())->Name == OPAAX_ID("Idle"));

        // A default naming a clip that was since removed must not strand the lookup either.
        AnimationLibraryData lStaleDefault = MakeProbeLibrary();
        lStaleDefault.DefaultClip = OPAAX_ID("Deleted");

        REQUIRE(lStaleDefault.Find(OpaaxStringID()) != nullptr);
        CHECK(lStaleDefault.Find(OpaaxStringID())->Name == OPAAX_ID("Idle"));
    }

    TEST_CASE("Find and FindExact: an empty library resolves nothing, either way")
    {
        const AnimationLibraryData lEmpty;

        CHECK(lEmpty.Find(OpaaxStringID()) == nullptr);
        CHECK(lEmpty.Find(OPAAX_ID("Idle")) == nullptr);
        CHECK(lEmpty.FindExact(OPAAX_ID("Idle")) == nullptr);
    }

    TEST_CASE("FindExact: no fallback at all, which is what a rename check needs")
    {
        const AnimationLibraryData lLibrary = MakeProbeLibrary();

        CHECK(lLibrary.FindExact(OPAAX_ID("Idle")) != nullptr);
        CHECK(lLibrary.FindExact(OpaaxStringID()) == nullptr);   // NOT the default
        CHECK(lLibrary.FindExact(OPAAX_ID("Nope")) == nullptr);
    }

    TEST_CASE("AnimationLibraryFile: save then load round-trips the names and the default")
    {
        const ScopedTempDir lDir("lib_roundtrip");
        const OpaaxString   lPath = lDir.Sub("Hero.opaaxanim");

        const AnimationLibraryData lSaved = MakeProbeLibrary();
        REQUIRE(AnimationLibraryFile::Save(lPath, lSaved));

        AnimationLibraryData lLoaded;
        REQUIRE(AnimationLibraryFile::Load(lPath, lLoaded));

        CHECK(lLoaded.EntryCount() == 3u);
        CHECK(lLoaded.DefaultClip == OPAAX_ID("Run"));
        REQUIRE(lLoaded.Find(OPAAX_ID("Idle")) != nullptr);
        CHECK(lLoaded.Find(OPAAX_ID("Idle"))->Clip.Path == OpaaxString("Anims/Idle.opaaxclip"));

        // Order is preserved, because "the first entry" is a documented fallback.
        CHECK(lLoaded.Entries[0].Name == OPAAX_ID("Idle"));
        CHECK(AnimationLibraryFile::Serialize(lLoaded) == AnimationLibraryFile::Serialize(lSaved));
    }

    TEST_CASE("AnimationLibraryFile: a malformed file leaves the caller's data untouched")
    {
        const ScopedTempDir lDir("lib_bad");

        AnimationLibraryData lExisting = MakeProbeLibrary();

        CHECK_FALSE(AnimationLibraryFile::Load(lDir.Sub("Nope.opaaxanim"), lExisting));
        CHECK(lExisting.EntryCount() == 3u);

        const OpaaxString lArray = lDir.Sub("Array.opaaxanim");
        REQUIRE(FileIO::WriteAllText(lArray, OpaaxString("[]")));

        CHECK_FALSE(AnimationLibraryFile::Load(lArray, lExisting));
        CHECK(lExisting.EntryCount() == 3u);
    }
}
