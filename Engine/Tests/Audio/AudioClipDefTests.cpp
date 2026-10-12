// Suite: clip definitions — the seeded random, each way of choosing a variation (never twice in a
// row, weights, in order, shuffled), the volume and pitch spread, the .opaaxdata format, a sound
// whose clip is still loading (it starts once loaded), and a Play world playing definitions by path
// and from sources.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioRandom.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Config/EngineConfigData.h"
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

using namespace Opaax;

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

    /** A mono 16-bit WAV: a sine at InHz, InSeconds long, peaking at 0.5. */
    TDynArray<Uint8> MakeWav(const float InSeconds, const float InHz = 440.f)
    {
        constexpr Uint32 RATE = 44100;
        const Uint32 lFrames  = static_cast<Uint32>(RATE * InSeconds);

        TDynArray<Uint8> lOut;
        const auto lPut32  = [&lOut](const Uint32 InValue) { for (int i = 0; i < 4; ++i) { lOut.push_back(static_cast<Uint8>(InValue >> (8 * i))); } };
        const auto lPut16  = [&lOut](const Uint16 InValue) { lOut.push_back(static_cast<Uint8>(InValue & 0xFF)); lOut.push_back(static_cast<Uint8>(InValue >> 8)); };
        const auto lPutTag = [&lOut](const char* InTag) { lOut.insert(lOut.end(), InTag, InTag + 4); };

        lPutTag("RIFF"); lPut32(36 + lFrames * 2); lPutTag("WAVE");
        lPutTag("fmt "); lPut32(16); lPut16(1); lPut16(1); lPut32(RATE); lPut32(RATE * 2); lPut16(2); lPut16(16);
        lPutTag("data"); lPut32(lFrames * 2);
        for (Uint32 lFrame = 0; lFrame < lFrames; ++lFrame)
        {
            lPut16(static_cast<Uint16>(static_cast<Int16>(0.5 * 32767.0 * std::sin(2.0 * 3.14159265358979 * InHz * lFrame / RATE))));
        }
        return lOut;
    }

    /** Runs nothing until asked: a load stays "loading" for as long as a test wants. */
    class ManualJobs final : public IJobSystem
    {
    public:
        JobHandle Submit(TFunction<void()> InWork) override { return Submit(Move(InWork), {}); }

        JobHandle Submit(TFunction<void()> InWork, TFunction<void()> InOnComplete) override
        {
            m_Jobs.push_back({ Move(InWork), Move(InOnComplete) });
            return JobHandle{};
        }

        void ParallelFor(const Uint32 InCount, const TFunction<void(Uint32)>& InBody, Uint32) override
        {
            for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex) { InBody(lIndex); }
        }

        void   Wait(const JobHandle&) override {}
        void   DrainCompletions() override {}
        Uint32 GetWorkerCount() const noexcept override { return 0; }

        /** Runs every queued job and its completion. */
        void RunAll()
        {
            TDynArray<std::pair<TFunction<void()>, TFunction<void()>>> lJobs = Move(m_Jobs);
            m_Jobs.clear();
            for (auto& [lWork, lDone] : lJobs)
            {
                if (lWork) { lWork(); }
                if (lDone) { lDone(); }
            }
        }

    private:
        TDynArray<std::pair<TFunction<void()>, TFunction<void()>>> m_Jobs;
    };

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
        TDynArray<Int32> lOne(lShuffled.begin() + static_cast<std::ptrdiff_t>(lRound),
                              lShuffled.begin() + static_cast<std::ptrdiff_t>(lRound + 4));
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

// =============================================================================
// A sound whose clip is still loading
// =============================================================================
TEST_CASE("AudioManager: a sound whose clip is loading plays once it is loaded; a stop before cancels it")
{
    const fs::path lFile = fs::temp_directory_path() / "OpaaxAudioPending.wav";
    {
        const TDynArray<Uint8> lWav = MakeWav(0.5f);
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
    class TempPaths final : public IPaths
    {
    public:
        explicit TempPaths(const fs::path& InRoot) : m_Root(InRoot) {}

        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel) const override
        {
            return OpaaxString((m_Root / InAssetRel.CStr()).generic_string().c_str());
        }

        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath) const override
        {
            return OpaaxString(fs::path(InAbsPath.CStr()).lexically_relative(m_Root).generic_string().c_str());
        }

        void        LogPaths()      const override {}
        OpaaxString WorkspaceRoot() const override { return Root(); }
        OpaaxString EngineRoot()    const override { return Root(); }
        OpaaxString ProjectRoot()   const override { return Root(); }
        OpaaxString ProjectFile()   const override { return Root(); }
        OpaaxString AssetsDir()     const override { return Root(); }
        OpaaxString ConfigsDir()    const override { return Root(); }
        OpaaxString SourceDir()     const override { return Root(); }
        OpaaxString SaveDir()       const override { return Root(); }
        OpaaxString TempDir()       const override { return Root(); }

        OpaaxString EngineToAbsolute(const OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        OpaaxString ProjectToAbsolute(const OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        OpaaxString Root() const { return OpaaxString(m_Root.generic_string().c_str()); }

        fs::path m_Root;
    };

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
                const TDynArray<Uint8> lWav = MakeWav(0.2f, 300.f + 100.f * lIndex);
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
            lSubsystems.RegisterSubsystem<BehaviourSubsystem>(std::ref(*TheWorld->GetContext()));
            lSubsystems.RegisterSubsystem<AudioSubsystem>(std::ref(*TheWorld->GetContext()));
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
