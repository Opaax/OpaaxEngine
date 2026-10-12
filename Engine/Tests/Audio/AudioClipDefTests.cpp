// Suite: clip definitions — the seeded random, each way of choosing a variation (never twice in a
// row, weights, in order, shuffled), the volume and pitch spread, the .opaaxdata format, a sound
// whose clip is still loading (it starts once loaded), and a Play world playing definitions by path
// and from sources.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "Core/OpaaxTypes.h"
#include <string>

#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"
#include "Audio/AudioBank.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioRandom.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioBanksComponent.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/DebugDraw.h"
#include "Resources/DataAsset/DataAsset.h"
#include "Resources/DataAsset/DataAssetFile.h"
#include "Resources/ResourceManager.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include "AudioTestHelpers.h"

using namespace Opaax;
using AudioTest::ManualJobs;
using AudioTest::MakeWav;
using AudioTest::TempPaths;

namespace
{
    namespace fs = std::filesystem;

    AudioClipDef MakeDef(const EAudioSelection InSelection, const Uint32 InCount)
    {
        AudioClipDef lDef;
        lDef.Selection = InSelection;
        for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex)
        {
            AudioClipEntry lEntry;
            lEntry.Clip.Path = OpaaxString(("Audio/Step" + std::to_string(lIndex) + ".wav").c_str());
            lDef.Clips.push_back(lEntry);
        }
        return lDef;
    }

    TDynArray<Int32> Picks(const AudioClipDef& InDef, const Uint32 InCount, const Uint64 InSeed = 7)
    {
        AudioRandom         lRandom(InSeed);
        AudioSelectionState lState;

        TDynArray<Int32> lPicks;
        for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex)
        {
            lPicks.push_back(SelectAudioClip(InDef, lState, lRandom));
        }
        return lPicks;
    }

    AudioManager::Desc Headless()
    {
        AudioManager::Desc lDesc;
        lDesc.bHeadless  = true;
        lDesc.SampleRate = 48000;
        lDesc.MaxVoices  = 16;
        return lDesc;
    }

    float Peak(AudioManager& InAudio, const float InSeconds = 0.05f)
    {
        const Uint64 lFrames = static_cast<Uint64>(InSeconds * 48000.f);
        TDynArray<float> lBuffer(static_cast<size_t>(lFrames * 2), 0.f);
        InAudio.RenderFrames(lBuffer.data(), lFrames);

        float lPeak = 0.f;
        for (const float lSample : lBuffer) { lPeak = std::max(lPeak, std::abs(lSample)); }
        return lPeak;
    }
}

// =============================================================================
// Random and selection
// =============================================================================
TEST_CASE("AudioRandom: the same seed gives the same numbers; ranges hold")
{
    AudioRandom lA(42);
    AudioRandom lB(42);
    AudioRandom lC(43);

    bool bSame      = true;
    bool bDifferent = false;
    for (int lIndex = 0; lIndex < 100; ++lIndex)
    {
        const Uint32 lValue = lA.NextUint32();
        bSame      = bSame && lValue == lB.NextUint32();
        bDifferent = bDifferent || lValue != lC.NextUint32();
    }
    CHECK(bSame);
    CHECK(bDifferent);

    for (int lIndex = 0; lIndex < 1000; ++lIndex)
    {
        const float lFloat = lA.Range(-0.1f, 0.1f);
        CHECK(lFloat >= -0.1f);
        CHECK(lFloat < 0.1f);
        CHECK(lA.Below(5) < 5);
    }
    CHECK(lA.Below(0) == 0);
}

TEST_CASE("AudioClipDef: five footsteps in RandomNoRepeat never play the same one twice in a row")
{
    const TDynArray<Int32> lPicks = Picks(MakeDef(EAudioSelection::RandomNoRepeat, 5), 1000);

    bool bRepeat = false;
    TDynArray<Uint32> lCounts(5, 0);
    for (size_t lIndex = 0; lIndex < lPicks.size(); ++lIndex)
    {
        REQUIRE(lPicks[lIndex] >= 0);
        REQUIRE(lPicks[lIndex] < 5);
        ++lCounts[lPicks[lIndex]];
        bRepeat = bRepeat || (lIndex > 0 && lPicks[lIndex] == lPicks[lIndex - 1]);
    }
    CHECK_FALSE(bRepeat);
    for (const Uint32 lCount : lCounts)
    {
        CHECK(lCount > 150);   // each about 200
    }

    // Seeded: the same seed, the same footsteps.
    CHECK(Picks(MakeDef(EAudioSelection::RandomNoRepeat, 5), 50, 3) == Picks(MakeDef(EAudioSelection::RandomNoRepeat, 5), 50, 3));
    CHECK(Picks(MakeDef(EAudioSelection::RandomNoRepeat, 5), 50, 3) != Picks(MakeDef(EAudioSelection::RandomNoRepeat, 5), 50, 4));

    // One clip: it plays every time rather than nothing.
    CHECK(Picks(MakeDef(EAudioSelection::RandomNoRepeat, 1), 3) == TDynArray<Int32>{ 0, 0, 0 });
}

TEST_CASE("AudioClipDef: Random follows the weights; a weight of 0 never plays")
{
    AudioClipDef lDef = MakeDef(EAudioSelection::Random, 3);
    lDef.Clips[0].Weight = 1.f;
    lDef.Clips[1].Weight = 0.f;
    lDef.Clips[2].Weight = 3.f;

    TDynArray<Uint32> lCounts(3, 0);
    for (const Int32 lPick : Picks(lDef, 4000))
    {
        ++lCounts[lPick];
    }
    CHECK(lCounts[1] == 0);
    CHECK(static_cast<float>(lCounts[2]) / static_cast<float>(lCounts[0]) == doctest::Approx(3.f).epsilon(0.15));

    // Every weight at 0: equally likely, rather than nothing.
    for (AudioClipEntry& lEntry : lDef.Clips) { lEntry.Weight = 0.f; }
    const TDynArray<Int32> lEqual = Picks(lDef, 300);
    CHECK(std::count(lEqual.begin(), lEqual.end(), 1) > 50);
}

TEST_CASE("AudioClipDef: Single, Sequential and Shuffle; no clips plays nothing")
{
    CHECK(Picks(MakeDef(EAudioSelection::Single, 3), 4) == TDynArray<Int32>{ 0, 0, 0, 0 });
    CHECK(Picks(MakeDef(EAudioSelection::Sequential, 3), 7) == TDynArray<Int32>{ 0, 1, 2, 0, 1, 2, 0 });
    CHECK(Picks(MakeDef(EAudioSelection::Random, 0), 2) == TDynArray<Int32>{ -1, -1 });

    // Shuffle: every round plays each clip once, and a round never starts with the last one played.
    const TDynArray<Int32> lShuffled = Picks(MakeDef(EAudioSelection::Shuffle, 4), 40);
    for (size_t lRound = 0; lRound < lShuffled.size(); lRound += 4)
    {
        TDynArray<Int32> lOne(lShuffled.begin() + static_cast<PtrDiff>(lRound),
                              lShuffled.begin() + static_cast<PtrDiff>(lRound + 4));
        std::sort(lOne.begin(), lOne.end());
        CHECK(lOne == TDynArray<Int32>{ 0, 1, 2, 3 });

        if (lRound > 0)
        {
            CHECK(lShuffled[lRound] != lShuffled[lRound - 1]);
        }
    }
}

TEST_CASE("AudioClipDef: each play takes the definition's settings, the clip's scales and a spread within its bounds")
{
    AudioClipDef lDef = MakeDef(EAudioSelection::Single, 1);
    lDef.Bus                  = OPAAX_ID("Dialogue");
    lDef.Volume               = 0.8f;
    lDef.Pitch                = 1.f;
    lDef.VolumeSpread         = 0.1f;
    lDef.PitchSpread          = 0.05f;
    lDef.bLoop                = true;
    lDef.bSpatial             = true;
    lDef.MaxDistance          = 900.f;
    lDef.Attenuation          = EAudioAttenuation::InverseSquare;
    lDef.Priority             = 300;   // above 255: the highest
    lDef.Clips[0].VolumeScale = 0.5f;

    AudioRandom lRandom(11);
    float lLowest  = 10.f;
    float lHighest = 0.f;
    for (int lIndex = 0; lIndex < 500; ++lIndex)
    {
        const PlaySoundParams lParams = MakePlaySoundParams(lDef, 0, lRandom);
        CHECK(lParams.Volume >= 0.8f * 0.5f * 0.9f - 1e-5f);
        CHECK(lParams.Volume <= 0.8f * 0.5f * 1.1f + 1e-5f);
        CHECK(lParams.Pitch >= 0.95f - 1e-5f);
        CHECK(lParams.Pitch <= 1.05f + 1e-5f);
        lLowest  = std::min(lLowest, lParams.Pitch);
        lHighest = std::max(lHighest, lParams.Pitch);
    }
    CHECK(lHighest - lLowest > 0.05f);   // it does spread

    const PlaySoundParams lParams = MakePlaySoundParams(lDef, 0, lRandom);
    CHECK(lParams.Bus == OPAAX_ID("Dialogue"));
    CHECK(lParams.bLoop);
    CHECK(lParams.bSpatial);
    CHECK(lParams.MaxDistance == 900.f);
    CHECK(lParams.Attenuation == EAudioAttenuation::InverseSquare);
    CHECK(lParams.Priority == 255);
}

TEST_CASE("AudioClipDef: the data asset round-trips; a file missing fields keeps their defaults")
{
    AudioClipDef lDef = MakeDef(EAudioSelection::Shuffle, 2);
    lDef.Clips[1].Weight = 2.5f;
    lDef.PitchSpread     = 0.05f;
    lDef.Bus             = OPAAX_ID("Music");

    const nlohmann::json lJson = lDef;
    CHECK(lJson.at("Selection") == "Shuffle");
    CHECK(lJson.at("Attenuation") == "Linear");   // enums by label, so reordering one is safe
    CHECK(lJson.at("Clips").at(1).at("Clip") == "Audio/Step1.wav");

    const TUniquePtr<TDataAssetObject<AudioClipDef>> lRead = ReadDataAsset<AudioClipDef>(lJson);
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Value.Clips.size() == 2);
    CHECK(lRead->Value.Clips[1].Weight == 2.5f);
    CHECK(lRead->Value.Bus == OPAAX_ID("Music"));

    const TUniquePtr<TDataAssetObject<AudioClipDef>> lOld = ReadDataAsset<AudioClipDef>(nlohmann::json{ { "Volume", 0.5f } });
    REQUIRE(lOld != nullptr);
    CHECK(lOld->Value.Volume == 0.5f);
    CHECK(lOld->Value.Selection == EAudioSelection::Random);
    CHECK(lOld->Value.Bus == OPAAX_ID("SFX"));
    CHECK(lOld->Value.Clips.empty());

    CHECK(DataAssetTypeName<AudioClipDef>() == OPAAX_ID("AudioClipDef"));
}

namespace
{
    /** Counts the fields no editor widget exists for, and the lists, opening every group and list. */
    class DrawableCheck final : public IPropertyVisitor
    {
    public:
        TDynArray<std::string> Unsupported;
        Uint32                 Lists = 0;

        void Visit(const char*, bool&, const PropertyMeta&) override          {}
        void Visit(const char*, Int16&, const PropertyMeta&) override         {}
        void Visit(const char*, Int32&, const PropertyMeta&) override         {}
        void Visit(const char*, Uint32&, const PropertyMeta&) override        {}
        void Visit(const char*, float&, const PropertyMeta&) override         {}
        void Visit(const char*, Vector2F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector3F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector4F&, const PropertyMeta&) override      {}
        void Visit(const char*, LinearColor&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}
        void VisitResourcePath(const char*, OpaaxString&, Uint32, const PropertyMeta&) override {}
        void VisitDataAssetRef(const char*, OpaaxString&, OpaaxStringID, const PropertyMeta&) override {}
        bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
        void EndGroup() override {}

        void VisitUnsupported(const char* InName, OpaaxStringView) override { Unsupported.emplace_back(InName); }

        void VisitList(const char* InName, IPropertyList& InList, const PropertyMeta& InMeta) override
        {
            ++Lists;
            InList.Add();   // an element to visit, so its fields are checked too
            IPropertyVisitor::VisitList(InName, InList, InMeta);
        }
    };

    template<typename T>
    DrawableCheck CheckDrawable()
    {
        T             lValue;
        DrawableCheck lCheck;
        VisitProperties(lValue, lCheck);
        return lCheck;
    }
}

TEST_CASE("Audio types: every field has an editor widget, lists and their elements included")
{
    const DrawableCheck lDef = CheckDrawable<AudioClipDef>();
    CHECK(lDef.Unsupported.empty());
    CHECK(lDef.Lists == 1);

    const DrawableCheck lBank = CheckDrawable<AudioBank>();
    CHECK(lBank.Unsupported.empty());
    CHECK(lBank.Lists == 2);

    CHECK(CheckDrawable<AudioSourceComponent>().Unsupported.empty());
    CHECK(CheckDrawable<AudioListenerComponent>().Unsupported.empty());
    CHECK(CheckDrawable<AudioBanksComponent>().Unsupported.empty());

    const DrawableCheck lSettings = CheckDrawable<AudioSettings>();
    CHECK(lSettings.Unsupported.empty());
    CHECK(lSettings.Lists == 1);   // the buses
}

// =============================================================================
// A sound whose clip is still loading
// =============================================================================
TEST_CASE("AudioManager: a sound whose clip is loading plays once it is loaded; a stop before cancels it")
{
    const fs::path lFile = fs::temp_directory_path() / "OpaaxAudioPending.wav";
    {
        const TDynArray<Uint8> lWav = MakeWav(44100, 0.5f);
        std::ofstream(lFile, std::ios::binary).write(reinterpret_cast<const char*>(lWav.data()), static_cast<std::streamsize>(lWav.size()));
    }

    ManualJobs      lJobs;
    ResourceManager lResources;
    lResources.SetJobSystem(lJobs);

    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());

    PlaySoundParams lParams;
    lParams.bLoop = true;

    SUBCASE("it starts when loaded, with what was asked meanwhile")
    {
        const ResourceRef<AudioClipResource> lClip = lResources.LoadAsync<AudioClipResource>(lFile.generic_string().c_str());
        const SoundHandle lSound = lAudio.Play(lClip, lParams);
        REQUIRE(lSound.IsValid());
        CHECK(lAudio.IsPlaying(lSound));   // from now
        lAudio.SetVolume(lSound, 0.5f);

        lAudio.Update(0.05);
        CHECK(Peak(lAudio) == 0.f);   // still loading: nothing heard

        lJobs.RunAll();
        lAudio.Update(0.0);
        CHECK(Peak(lAudio) == doctest::Approx(0.25f).epsilon(0.05));
    }

    SUBCASE("a stop before it loads: it never plays")
    {
        const ResourceRef<AudioClipResource> lClip = lResources.LoadAsync<AudioClipResource>(lFile.generic_string().c_str());
        const SoundHandle lSound = lAudio.Play(lClip, lParams);
        lAudio.Stop(lSound);
        CHECK_FALSE(lAudio.IsPlaying(lSound));
        CHECK(lAudio.GetVoiceCount() == 0);

        lJobs.RunAll();
        lAudio.Update(0.0);
        CHECK(Peak(lAudio) == 0.f);
    }

    SUBCASE("a clip that fails to load frees its voice")
    {
        const ResourceRef<AudioClipResource> lClip = lResources.LoadAsync<AudioClipResource>(
            (fs::temp_directory_path() / "OpaaxAudioNope.wav").generic_string().c_str());
        const SoundHandle lSound = lAudio.Play(lClip, lParams);

        lJobs.RunAll();
        lAudio.Update(0.0);
        CHECK_FALSE(lAudio.IsPlaying(lSound));
        CHECK(lAudio.GetVoiceCount() == 0);
    }

    lAudio.Shutdown();
    std::error_code lError;
    fs::remove(lFile, lError);
}

// =============================================================================
// A Play world's clip definitions
// =============================================================================
namespace
{
    /** Audio/Step0..2.wav, Audio/Steps.opaaxdata (RandomNoRepeat over them) and Audio/Empty.opaaxdata. */
    struct DefWorldFixture
    {
        fs::path          Root = fs::temp_directory_path() / "OpaaxAudioDefTests";
        TempPaths         Paths{ Root };
        ResourceManager   Resources;
        EngineEventBus    Events;
        DebugDraw         Debug;
        EngineConfigData  Config;
        InputManager      Input;
        AudioManager      Audio{ Headless() };
        ComponentRegistry Components;
        WorldManager      Worlds;   // last: its worlds end before the audio

        World*          TheWorld = nullptr;
        AudioSubsystem* Sounds   = nullptr;

        DefWorldFixture()
        {
            std::error_code lError;
            fs::remove_all(Root, lError);
            fs::create_directories(Root / "Audio", lError);

            AudioClipDef lSteps = MakeDef(EAudioSelection::RandomNoRepeat, 3);
            for (Uint32 lIndex = 0; lIndex < 3; ++lIndex)
            {
                const TDynArray<Uint8> lWav = MakeWav(44100, 0.2f, 300.f + 100.f * lIndex);
                std::ofstream(Root / "Audio" / ("Step" + std::to_string(lIndex) + ".wav"), std::ios::binary)
                    .write(reinterpret_cast<const char*>(lWav.data()), static_cast<std::streamsize>(lWav.size()));
            }
            lSteps.Volume = 0.5f;

            REQUIRE(DataAssetFile::Save(OpaaxString((Root / "Audio" / "Steps.opaaxdata").generic_string().c_str()),
                                        DataAssetTypeName<AudioClipDef>(), nlohmann::json(lSteps)));
            REQUIRE(DataAssetFile::Save(OpaaxString((Root / "Audio" / "Empty.opaaxdata").generic_string().c_str()),
                                        DataAssetTypeName<AudioClipDef>(), nlohmann::json(AudioClipDef{})));

            REQUIRE(Audio.Startup());
            REQUIRE(Components.Register<TransformComponent>(OpaaxStringID("Transform"), /*bEssential*/true));

            TheWorld = Worlds.CreateWorld("Steps", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);
            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, Paths, Events, Input, Config,
                                               /*Actions*/ nullptr, /*UI*/ nullptr, Debug, &Components, &Audio });

            WorldSubsystemMgr& lSubsystems = TheWorld->GetSubsystems();
            lSubsystems.RegisterSubsystem<BehaviourSubsystem>(Ref(*TheWorld->GetContext()));
            lSubsystems.RegisterSubsystem<AudioSubsystem>(Ref(*TheWorld->GetContext()));
            lSubsystems.StartupAll();

            Sounds = lSubsystems.GetSubsystem<AudioSubsystem>();
            REQUIRE(Sounds != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        ~DefWorldFixture()
        {
            Worlds.DestroyWorld(TheWorld);
            std::error_code lError;
            fs::remove_all(Root, lError);
        }

        void Frame(const double InDeltaTime = 1.0 / 60.0)
        {
            Worlds.Update(InDeltaTime);
            Worlds.FixedUpdate(InDeltaTime);
            Audio.Update(InDeltaTime);
        }
    };
}

TEST_CASE("AudioSubsystem: a clip definition plays by path, one of its clips at its volume")
{
    DefWorldFixture lFix;

    const SoundHandle lSound = lFix.Sounds->PlaySound(OpaaxString("Audio/Steps.opaaxdata"));
    REQUIRE(lSound.IsValid());
    CHECK(lFix.Audio.IsPlaying(lSound));
    CHECK(Peak(lFix.Audio) == doctest::Approx(0.25f).epsilon(0.05));   // 0.5 x the definition's 0.5

    // Its volume scaled per play.
    PlaySoundParams lQuiet;
    lQuiet.Volume = 0.5f;
    lFix.Audio.Stop(lSound);
    lFix.Frame();
    lFix.Sounds->PlaySound(OpaaxString("Audio/Steps.opaaxdata"), lQuiet);
    CHECK(Peak(lFix.Audio) == doctest::Approx(0.125f).epsilon(0.05));

    // A definition with no clips, or that is not there: nothing plays, nothing breaks.
    CHECK_FALSE(lFix.Sounds->PlaySound(OpaaxString("Audio/Empty.opaaxdata")).IsValid());
    CHECK_FALSE(lFix.Sounds->PlaySound(OpaaxString("Audio/Nope.opaaxdata")).IsValid());
}

TEST_CASE("AudioSubsystem: a source plays its definition; with bStopOnDestroy off it outlives its entity")
{
    DefWorldFixture lFix;

    Entity lSpeaker = lFix.TheWorld->CreateEntity("Speaker");
    AudioSourceComponent lSource;
    lSource.Sound.Path = OpaaxString("Audio/Steps.opaaxdata");
    lSource.bSpatial   = false;
    lSpeaker.AddOrReplace<AudioSourceComponent>(lSource);

    lFix.Frame();
    REQUIRE(lFix.Sounds->IsSourcePlaying(lSpeaker.GetHandle()));
    CHECK(lFix.Audio.GetVoiceCount() == 1);

    SUBCASE("stops with its entity")
    {
        lFix.TheWorld->DestroyEntity(lSpeaker);
        CHECK(lFix.Audio.GetVoiceCount() == 0);
    }

    SUBCASE("plays on after its entity")
    {
        lSpeaker.Get<AudioSourceComponent>().bStopOnDestroy = false;
        lFix.TheWorld->DestroyEntity(lSpeaker);
        CHECK(lFix.Audio.GetVoiceCount() == 1);
        CHECK(lFix.Sounds->GetPlayingCount() == 1);

        // It plays to its end (the clips are 0.2 s).
        lFix.Frame(0.3);
        CHECK(lFix.Audio.GetVoiceCount() == 0);
    }
}
