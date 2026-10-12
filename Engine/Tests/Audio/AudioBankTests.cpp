// Suite: audio banks — the data asset, a hold loading every sound of a bank in the background and
// letting them go, a level's AudioBanks components (a banked sound plays without the "not loaded"
// warning, the banks go with their entity), and the global bank across worlds that come and go: its
// sounds are never loaded again, a level's are let go each time.
#include <doctest.h>

#include <functional>
#include <string>

#include "Audio/AudioBank.h"
#include "Audio/AudioBankHold.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioGlobalBank.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioBanksComponent.h"
#include "Core/Log/Logger.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstanceContext.h"
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

    void SaveData(const fs::path& InPath, const OpaaxStringID InType, const nlohmann::json& InData)
    {
        fs::create_directories(InPath.parent_path());
        REQUIRE(DataAssetFile::Save(OpaaxString(InPath.generic_string().c_str()), InType, InData));
    }

    /**
     * Audio/A,B,C,D,G,L.wav; Audio/Steps.opaaxdata (A and B); Audio/Bank.opaaxdata (Steps, C and A
     * again); Audio/Global.opaaxdata (G); Audio/Level.opaaxdata (L).
     */
    struct BankAssets
    {
        fs::path  Root = fs::temp_directory_path() / "OpaaxAudioBankTests";
        TempPaths Paths{ Root };

        BankAssets()
        {
            std::error_code lError;
            fs::remove_all(Root, lError);

            for (const char* lName : { "A", "B", "C", "D", "G", "L" })
            {
                AudioTest::WriteFile(Root / "Audio" / (std::string(lName) + ".wav"), MakeWav(44100, 0.2f));
            }

            AudioClipDef lSteps;
            lSteps.Clips.resize(2);
            lSteps.Clips[0].Clip.Path = OpaaxString("Audio/A.wav");
            lSteps.Clips[1].Clip.Path = OpaaxString("Audio/B.wav");
            SaveData(Root / "Audio" / "Steps.opaaxdata", DataAssetTypeName<AudioClipDef>(), nlohmann::json(lSteps));

            AudioBank lBank;
            lBank.ClipDefs.resize(1);
            lBank.ClipDefs[0].Path = OpaaxString("Audio/Steps.opaaxdata");
            lBank.Clips.resize(2);
            lBank.Clips[0].Path = OpaaxString("Audio/C.wav");
            lBank.Clips[1].Path = OpaaxString("Audio/A.wav");
            SaveData(Root / "Audio" / "Bank.opaaxdata", DataAssetTypeName<AudioBank>(), nlohmann::json(lBank));

            AudioBank lGlobal;
            lGlobal.Clips.resize(1);
            lGlobal.Clips[0].Path = OpaaxString("Audio/G.wav");
            SaveData(Root / "Audio" / "Global.opaaxdata", DataAssetTypeName<AudioBank>(), nlohmann::json(lGlobal));

            AudioBank lLevel;
            lLevel.Clips.resize(1);
            lLevel.Clips[0].Path = OpaaxString("Audio/L.wav");
            SaveData(Root / "Audio" / "Level.opaaxdata", DataAssetTypeName<AudioBank>(), nlohmann::json(lLevel));
        }

        ~BankAssets()
        {
            std::error_code lError;
            fs::remove_all(Root, lError);
        }

        OpaaxString Absolute(const char* InAsset) const { return Paths.AssetToAbsolute(OpaaxString(InAsset)); }
    };

    bool IsResident(ResourceManager& InResources, const BankAssets& InAssets, const char* InAsset)
    {
        return InResources.Find<AudioClipResource>(InAssets.Absolute(InAsset).CStr()).IsValid();
    }

    /** Log lines since the last call that contain InText. */
    class LogWatch
    {
    public:
        LogWatch()
        {
            Logger::Get().EnableHistory(512);
            TDynArray<LogEntry> lSkip;
            m_Last = Logger::Get().CopyHistorySince(0, lSkip);
        }

        Uint32 Count(const char* InText)
        {
            TDynArray<LogEntry> lEntries;
            m_Last = Logger::Get().CopyHistorySince(m_Last, lEntries);

            Uint32 lCount = 0;
            for (const LogEntry& lEntry : lEntries)
            {
                if (std::string(lEntry.Message.CStr()).find(InText) != std::string::npos) { ++lCount; }
            }
            return lCount;
        }

    private:
        Uint64 m_Last = 0;
    };

    AudioManager::Desc Headless()
    {
        AudioManager::Desc lDesc;
        lDesc.bHeadless  = true;
        lDesc.SampleRate = 48000;
        lDesc.MaxVoices  = 16;
        return lDesc;
    }

    /** A headless audio, the shared engine parts, and Play worlds made on demand. */
    struct BankWorlds
    {
        BankAssets        Assets;
        ManualJobs        Jobs;
        ResourceManager   Resources;
        EngineEventBus    Events;
        DebugDraw         Debug;
        EngineConfigData  Config;
        InputManager      Input;
        AudioManager      Audio{ Headless() };
        ComponentRegistry Components;
        WorldManager      Worlds;   // last: its worlds end before the audio

        BankWorlds()
        {
            Resources.SetJobSystem(Jobs);
            REQUIRE(Audio.Startup());
            REQUIRE(Components.Register<TransformComponent>(OpaaxStringID("Transform"), /*bEssential*/true));
        }

        /** A Play world with one entity holding InBank. */
        World* MakeWorld(const char* InName, const char* InBank, Entity* OutHolder = nullptr)
        {
            World* lWorld = Worlds.CreateWorld(InName, EWorldMode::Play);
            REQUIRE(lWorld != nullptr);
            lWorld->SetContext(WorldContext{ *lWorld, Resources, Assets.Paths, Events, Input, Config,
                                             /*Actions*/ nullptr, /*UI*/ nullptr, Debug, &Components, &Audio });

            WorldSubsystemMgr& lSubsystems = lWorld->GetSubsystems();
            lSubsystems.RegisterSubsystem<BehaviourSubsystem>(std::ref(*lWorld->GetContext()));
            lSubsystems.RegisterSubsystem<AudioSubsystem>(std::ref(*lWorld->GetContext()));
            lSubsystems.StartupAll();
            REQUIRE(Worlds.SetActiveWorld(lWorld));

            Entity lHolder = lWorld->CreateEntity("Banks");
            AudioBanksComponent lBanks;
            lBanks.Banks.resize(1);
            lBanks.Banks[0].Path = OpaaxString(InBank);
            lHolder.AddOrReplace<AudioBanksComponent>(lBanks);
            if (OutHolder != nullptr) { *OutHolder = lHolder; }
            return lWorld;
        }

        void Frame(const double InDeltaTime = 1.0 / 60.0)
        {
            Worlds.Update(InDeltaTime);
            Worlds.FixedUpdate(InDeltaTime);
            Audio.Update(InDeltaTime);
            Resources.Update(InDeltaTime);
        }
    };
}

// =============================================================================
// The bank and its hold
// =============================================================================
TEST_CASE("AudioBank: the data asset round-trips; a file missing fields keeps their defaults")
{
    AudioBank lBank;
    lBank.ClipDefs.resize(1);
    lBank.ClipDefs[0].Path = OpaaxString("Audio/Steps.opaaxdata");
    lBank.Clips.resize(1);
    lBank.Clips[0].Path = OpaaxString("Audio/Jump.wav");

    const nlohmann::json lJson = lBank;
    CHECK(lJson.at("ClipDefs").at(0) == "Audio/Steps.opaaxdata");

    const TUniquePtr<TDataAssetObject<AudioBank>> lRead = ReadDataAsset<AudioBank>(lJson);
    REQUIRE(lRead != nullptr);
    CHECK(lRead->Value.Clips[0].Path == "Audio/Jump.wav");

    const TUniquePtr<TDataAssetObject<AudioBank>> lEmpty = ReadDataAsset<AudioBank>(nlohmann::json::object());
    REQUIRE(lEmpty != nullptr);
    CHECK(lEmpty->Value.ClipDefs.empty());
    CHECK(DataAssetTypeName<AudioBank>() == OPAAX_ID("AudioBank"));
}

TEST_CASE("AudioBankHold: every sound of a bank loads in the background, once; releasing lets them go")
{
    const BankAssets lAssets;
    ManualJobs       lJobs;
    ResourceManager  lResources;
    lResources.SetJobSystem(lJobs);

    AudioBankHold lHold;
    REQUIRE(lHold.Hold(OpaaxString("Audio/Bank.opaaxdata"), lResources, lAssets.Paths));
    CHECK(lHold.GetClipCount() == 3);   // A, B (the definition) and C; A listed twice is held once
    CHECK(lHold.FindClip(OpaaxString("Audio/B.wav")) != nullptr);
    CHECK(lHold.FindClip(OpaaxString("Audio/D.wav")) == nullptr);

    // Queued: nothing loaded on the spot.
    CHECK(lJobs.HasJobs());
    CHECK_FALSE(IsResident(lResources, lAssets, "Audio/C.wav"));

    lJobs.RunAll();
    CHECK(IsResident(lResources, lAssets, "Audio/A.wav"));
    CHECK(IsResident(lResources, lAssets, "Audio/B.wav"));
    CHECK(IsResident(lResources, lAssets, "Audio/C.wav"));
    CHECK_FALSE(lHold.FindClip(OpaaxString("Audio/C.wav"))->Get()->IsSilent());

    lHold.Release();
    lResources.Update(0.0);
    CHECK_FALSE(IsResident(lResources, lAssets, "Audio/C.wav"));

    CHECK_FALSE(lHold.Hold(OpaaxString("Audio/Nope.opaaxdata"), lResources, lAssets.Paths));
}

// =============================================================================
// Banks in worlds
// =============================================================================
TEST_CASE("AudioSubsystem: a level's banked sound plays without the warning; the banks go with their entity")
{
    BankWorlds lFix;
    Entity     lHolder;
    World*     lWorld = lFix.MakeWorld("Level", "Audio/Bank.opaaxdata", &lHolder);
    AudioSubsystem* lSounds = lWorld->GetSubsystems().GetSubsystem<AudioSubsystem>();

    LogWatch lLog;

    // Before the first frame (a behaviour's OnStart): the bank is found, its sound waits for its load.
    const SoundHandle lBanked = lSounds->PlaySound(OpaaxString("Audio/C.wav"));
    CHECK(lBanked.IsValid());
    CHECK(lLog.Count("was not loaded when played") == 0);

    // Not in a bank: logged, and it still plays.
    const SoundHandle lLoose = lSounds->PlaySound(OpaaxString("Audio/D.wav"));
    CHECK(lLoose.IsValid());
    CHECK(lLog.Count("was not loaded when played") == 1);

    lFix.Jobs.RunAll();
    lFix.Frame();
    CHECK(lFix.Audio.IsPlaying(lBanked));
    CHECK(lFix.Audio.IsPlaying(lLoose));

    // B is only in the bank (never played): it goes with the entity holding the bank.
    CHECK(IsResident(lFix.Resources, lFix.Assets, "Audio/B.wav"));
    lWorld->DestroyEntity(lHolder);
    lFix.Frame();
    CHECK_FALSE(IsResident(lFix.Resources, lFix.Assets, "Audio/B.wav"));
}

TEST_CASE("AudioGlobalBank: across worlds that come and go, its sounds are never loaded again; a level's go each time")
{
    BankWorlds lFix;
    lFix.Config.Audio.GlobalBank.Path = OpaaxString("Audio/Global.opaaxdata");

    GameInstanceContext lGameContext{ lFix.Worlds, lFix.Resources, lFix.Assets.Paths, lFix.Events, lFix.Input, lFix.Config };
    AudioGlobalBank     lGlobal(lGameContext);
    REQUIRE(lGlobal.Startup());
    lFix.Jobs.RunAll();

    const ResourceRef<AudioClipResource> lFirst = lFix.Resources.Find<AudioClipResource>(lFix.Assets.Absolute("Audio/G.wav").CStr());
    REQUIRE(lFirst.IsValid());
    const Uint32 lLoadedBefore = lFix.Resources.GetLoadedCount<AudioClipResource>();

    bool bGlobalKept  = true;
    bool bLevelLoaded = true;
    bool bLevelGone   = true;
    for (int lRound = 0; lRound < 5; ++lRound)
    {
        World* lWorld = lFix.MakeWorld(("Level" + std::to_string(lRound)).c_str(), "Audio/Level.opaaxdata");
        lFix.Frame();
        lFix.Jobs.RunAll();
        lFix.Frame();
        bLevelLoaded = bLevelLoaded && IsResident(lFix.Resources, lFix.Assets, "Audio/L.wav");

        lFix.Worlds.DestroyWorld(lWorld);
        lFix.Frame();
        bLevelGone = bLevelGone && !IsResident(lFix.Resources, lFix.Assets, "Audio/L.wav");

        // The same slot every round: the global sound was never released nor loaded again.
        const ResourceRef<AudioClipResource> lNow = lFix.Resources.Find<AudioClipResource>(lFix.Assets.Absolute("Audio/G.wav").CStr());
        bGlobalKept = bGlobalKept && lNow.IsValid() && lNow.GetHandle() == lFirst.GetHandle();
    }

    CHECK(bLevelLoaded);
    CHECK(bLevelGone);
    CHECK(bGlobalKept);
    CHECK(lFix.Resources.GetLoadedCount<AudioClipResource>() == lLoadedBefore);

    lGlobal.Shutdown();
}
