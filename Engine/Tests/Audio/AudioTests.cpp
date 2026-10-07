// Suite: audio — clips (format, failure policy), the mixer (sounds that end, loop, stop, volumes,
// spatial attenuation and panning, the voice limit) and a Play world's sounds (sources that start
// with their entity, follow it and stop with it; behaviours that play clips). Everything runs on a
// headless mixer: what would be heard is rendered into a buffer and measured. The sounds are
// generated WAV files.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "Application/Services/IPaths.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldEvents.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    constexpr Uint32 MIX_RATE = 48000;

    /** A mono 16-bit PCM WAV file: a sine at InHz, InSeconds long, peaking at InAmplitude. */
    TDynArray<Uint8> MakeWav(const Uint32 InSampleRate, const float InSeconds, const float InHz = 440.f,
                             const float InAmplitude = 0.5f)
    {
        const Uint32 lFrames    = static_cast<Uint32>(static_cast<float>(InSampleRate) * InSeconds);
        const Uint32 lDataBytes = lFrames * 2;

        TDynArray<Uint8> lOut;
        const auto lPut32  = [&lOut](const Uint32 InValue) { for (int i = 0; i < 4; ++i) { lOut.push_back(static_cast<Uint8>(InValue >> (8 * i))); } };
        const auto lPut16  = [&lOut](const Uint16 InValue) { lOut.push_back(static_cast<Uint8>(InValue & 0xFF)); lOut.push_back(static_cast<Uint8>(InValue >> 8)); };
        const auto lPutTag = [&lOut](const char* InTag) { lOut.insert(lOut.end(), InTag, InTag + 4); };

        lPutTag("RIFF"); lPut32(36 + lDataBytes); lPutTag("WAVE");
        lPutTag("fmt "); lPut32(16); lPut16(1); lPut16(1); lPut32(InSampleRate); lPut32(InSampleRate * 2); lPut16(2); lPut16(16);
        lPutTag("data"); lPut32(lDataBytes);

        for (Uint32 lFrame = 0; lFrame < lFrames; ++lFrame)
        {
            const double lPhase  = 2.0 * 3.14159265358979 * InHz * lFrame / InSampleRate;
            const auto   lSample = static_cast<Int16>(InAmplitude * 32767.0 * std::sin(lPhase));
            lPut16(static_cast<Uint16>(lSample));
        }

        return lOut;
    }

    AudioClipResource MakeClip(const float InSeconds, const float InAmplitude = 0.5f)
    {
        std::optional<AudioClipResource> lClip = AudioClipResource::FromMemory(MakeWav(44100, InSeconds, 440.f, InAmplitude), "Test");
        REQUIRE(lClip.has_value());
        return *lClip;
    }

    AudioManager::Desc Headless(const Uint32 InMaxVoices = 16)
    {
        AudioManager::Desc lDesc;
        lDesc.bHeadless  = true;
        lDesc.SampleRate = MIX_RATE;
        lDesc.Channels   = 2;
        lDesc.MaxVoices  = InMaxVoices;
        return lDesc;
    }

    struct Peaks
    {
        float Left  = 0.f;
        float Right = 0.f;

        float Max() const { return std::max(Left, Right); }
    };

    /** Renders InSeconds of the mix and measures the loudest sample of each channel. */
    Peaks RenderPeaks(AudioManager& InAudio, const float InSeconds = 0.05f)
    {
        const Uint64 lFrames = static_cast<Uint64>(InSeconds * MIX_RATE);
        TDynArray<float> lBuffer(static_cast<size_t>(lFrames * InAudio.GetChannels()), 0.f);
        REQUIRE(InAudio.RenderFrames(lBuffer.data(), lFrames) == lFrames);

        Peaks lPeaks;
        for (Uint64 lFrame = 0; lFrame < lFrames; ++lFrame)
        {
            lPeaks.Left  = std::max(lPeaks.Left,  std::abs(lBuffer[static_cast<size_t>(lFrame * 2)]));
            lPeaks.Right = std::max(lPeaks.Right, std::abs(lBuffer[static_cast<size_t>(lFrame * 2 + 1)]));
        }
        return lPeaks;
    }

    // Every asset under one temp root, so AssetToAbsolute is a concatenation.
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

    /** A temp folder with Audio/Beep.wav (0.25 s) and Audio/NotASound.wav, removed afterwards. */
    class AudioAssets
    {
    public:
        explicit AudioAssets(const char* InTag)
            : m_Root(fs::temp_directory_path() / ("OpaaxAudioTests_" + std::string(InTag)))
        {
            std::error_code lError;
            fs::remove_all(m_Root, lError);
            fs::create_directories(m_Root / "Audio", lError);

            const TDynArray<Uint8> lBeep = MakeWav(44100, 0.25f);
            std::ofstream(m_Root / "Audio" / "Beep.wav", std::ios::binary)
                .write(reinterpret_cast<const char*>(lBeep.data()), static_cast<std::streamsize>(lBeep.size()));
            std::ofstream(m_Root / "Audio" / "NotASound.wav", std::ios::binary) << "this is not audio";
        }

        ~AudioAssets()
        {
            std::error_code lError;
            fs::remove_all(m_Root, lError);
        }

        AudioAssets(const AudioAssets&)            = delete;
        AudioAssets& operator=(const AudioAssets&) = delete;

        const fs::path& Root() const { return m_Root; }

    private:
        fs::path m_Root;
    };
}

// =============================================================================
// Clips
// =============================================================================
TEST_CASE("AudioClip: a WAV file loads with its format and length")
{
    const AudioClipResource lClip = MakeClip(0.25f);

    CHECK_FALSE(lClip.IsSilent());
    CHECK(lClip.Channels == 1);
    CHECK(lClip.SampleRate == 44100);
    CHECK(lClip.FrameCount == 11025);
    CHECK(lClip.GetDuration() == doctest::Approx(0.25f));
    CHECK(lClip.ByteSize() == 44 + 11025 * 2);
}

TEST_CASE("AudioClip: bytes that are not a sound are refused; a missing file loads as silence")
{
    TDynArray<Uint8> lText = { 'n', 'o', 't', ' ', 'a', 'u', 'd', 'i', 'o' };
    CHECK_FALSE(AudioClipResource::FromMemory(lText, "Text").has_value());
    CHECK_FALSE(AudioClipResource::FromMemory({}, "Empty").has_value());

    // Through the resource manager: the placeholder policy gives a silent clip, never null.
    const AudioAssets lAssets("clips");
    ResourceManager   lResources;

    const ResourceRef<AudioClipResource> lGood = lResources.Load<AudioClipResource>((lAssets.Root() / "Audio" / "Beep.wav").generic_string().c_str());
    REQUIRE(lGood.Get() != nullptr);
    CHECK(lGood->FrameCount == 11025);

    const ResourceRef<AudioClipResource> lBad = lResources.Load<AudioClipResource>((lAssets.Root() / "Audio" / "NotASound.wav").generic_string().c_str());
    REQUIRE(lBad.Get() != nullptr);
    CHECK(lBad->IsSilent());

    const ResourceRef<AudioClipResource> lMissing = lResources.Load<AudioClipResource>((lAssets.Root() / "Audio" / "Nope.wav").generic_string().c_str());
    REQUIRE(lMissing.Get() != nullptr);
    CHECK(lMissing->IsSilent());
}

// =============================================================================
// Mixer
// =============================================================================
TEST_CASE("AudioManager: a sound plays for its length, then is released")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());
    CHECK(lAudio.IsHeadless());

    const SoundHandle lSound = lAudio.Play(MakeClip(0.25f));
    REQUIRE(lSound.IsValid());
    CHECK(lAudio.IsPlaying(lSound));
    CHECK(lAudio.GetVoiceCount() == 1);

    lAudio.Update(0.1);
    CHECK(lAudio.IsPlaying(lSound));

    lAudio.Update(0.2);   // 0.3 s in
    CHECK_FALSE(lAudio.IsPlaying(lSound));
    CHECK(lAudio.GetVoiceCount() == 0);

    // A finished sound's handle is harmless.
    lAudio.Stop(lSound);
    lAudio.SetVolume(lSound, 0.5f);
}

TEST_CASE("AudioManager: a looping sound plays until it is stopped")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());

    PlaySoundParams lParams;
    lParams.bLoop = true;
    const SoundHandle lSound = lAudio.Play(MakeClip(0.1f), lParams);

    lAudio.Update(1.0);
    CHECK(lAudio.IsPlaying(lSound));
    CHECK(RenderPeaks(lAudio).Max() > 0.1f);

    lAudio.Stop(lSound);
    CHECK_FALSE(lAudio.IsPlaying(lSound));
    CHECK(lAudio.GetVoiceCount() == 0);
    CHECK(RenderPeaks(lAudio).Max() == 0.f);
}

TEST_CASE("AudioManager: what is heard follows the sound, bus and master volumes")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());

    PlaySoundParams lParams;
    lParams.bLoop = true;
    lParams.Bus   = EAudioBus::Effects;
    const SoundHandle lSound = lAudio.Play(MakeClip(0.5f), lParams);

    const float lFull = RenderPeaks(lAudio).Max();
    REQUIRE(lFull > 0.2f);

    SUBCASE("the bus")
    {
        lAudio.SetBusVolume(EAudioBus::Effects, 0.f);
        CHECK(RenderPeaks(lAudio).Max() < 1e-4f);

        // Another bus is not affected.
        lAudio.SetBusVolume(EAudioBus::Effects, 1.f);
        lAudio.SetBusVolume(EAudioBus::Music, 0.f);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull).epsilon(0.05));
    }

    SUBCASE("the master")
    {
        lAudio.SetMasterVolume(0.5f);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.5f).epsilon(0.05));
    }

    SUBCASE("the sound")
    {
        lAudio.SetVolume(lSound, 0.25f);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.25f).epsilon(0.05));
    }

    SUBCASE("the settings")
    {
        AudioSettings lSettings;
        lSettings.MasterVolume  = 1.f;
        lSettings.EffectsVolume = 0.5f;
        lAudio.ApplySettings(lSettings);
        CHECK(lAudio.GetBusVolume(EAudioBus::Effects) == 0.5f);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.5f).epsilon(0.05));
    }
}

TEST_CASE("AudioManager: a spatial sound is panned toward its side and silent beyond its max distance")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());
    lAudio.SetListenerPosition({ 0.f, 0.f });

    PlaySoundParams lParams;
    lParams.bLoop       = true;
    lParams.bSpatial    = true;
    lParams.MinDistance = 100.f;
    lParams.MaxDistance = 1000.f;

    SUBCASE("to the right")
    {
        lParams.Position = { 300.f, 0.f };
        lAudio.Play(MakeClip(0.5f), lParams);

        const Peaks lPeaks = RenderPeaks(lAudio);
        CHECK(lPeaks.Right > 0.01f);
        CHECK(lPeaks.Right > lPeaks.Left);
    }

    SUBCASE("to the left")
    {
        lParams.Position = { -300.f, 0.f };
        lAudio.Play(MakeClip(0.5f), lParams);

        const Peaks lPeaks = RenderPeaks(lAudio);
        CHECK(lPeaks.Left > lPeaks.Right);
    }

    SUBCASE("too far")
    {
        lParams.Position = { 5000.f, 0.f };
        lAudio.Play(MakeClip(0.5f), lParams);
        CHECK(RenderPeaks(lAudio).Max() < 1e-4f);
    }

    SUBCASE("the listener moved next to it")
    {
        lParams.Position = { 5000.f, 0.f };
        lAudio.Play(MakeClip(0.5f), lParams);
        lAudio.SetListenerPosition({ 5000.f, 0.f });
        CHECK(RenderPeaks(lAudio).Max() > 0.1f);
    }
}

TEST_CASE("AudioManager: sounds over the voice limit, silent clips and a stopped mixer play nothing")
{
    AudioManager lAudio(Headless(/*InMaxVoices*/2));

    // Not started yet.
    CHECK_FALSE(lAudio.Play(MakeClip(0.1f)).IsValid());

    REQUIRE(lAudio.Startup());
    CHECK_FALSE(lAudio.Play(AudioClipResource::Placeholder()).IsValid());

    CHECK(lAudio.Play(MakeClip(0.1f)).IsValid());
    CHECK(lAudio.Play(MakeClip(0.1f)).IsValid());
    CHECK_FALSE(lAudio.Play(MakeClip(0.1f)).IsValid());   // the limit
    CHECK(lAudio.GetVoiceCount() == 2);

    lAudio.Shutdown();
    CHECK_FALSE(lAudio.IsStarted());
    CHECK(lAudio.GetVoiceCount() == 0);
}

TEST_CASE("AudioManager: a playing sound keeps its clip's bytes after the clip is gone")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());

    SoundHandle lSound;
    {
        PlaySoundParams lParams;
        lParams.bLoop = true;
        const AudioClipResource lClip = MakeClip(0.2f);
        lSound = lAudio.Play(lClip, lParams);
    }

    CHECK(lAudio.IsPlaying(lSound));
    CHECK(RenderPeaks(lAudio).Max() > 0.1f);
}

// =============================================================================
// A Play world's sounds
// =============================================================================
namespace AudioProbes
{
    /** Plays the beep when it starts. */
    struct Chime final : Behaviour
    {
        SoundHandle Sound;

        void OnStart() override { Sound = PlaySound("Audio/Beep.wav"); }
    };
}

namespace
{
    struct AudioWorldFixture
    {
        AudioAssets       Assets;
        TempPaths         Paths;
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

        AudioWorldFixture()
            : Assets("world")
            , Paths(Assets.Root())
        {
            REQUIRE(Audio.Startup());
            REQUIRE(Components.Register<TransformComponent>(OpaaxStringID("Transform"), /*bEssential*/true));
            REQUIRE(Components.Register<AudioProbes::Chime>(OpaaxStringID("Chime")));

            TheWorld = Worlds.CreateWorld("Sounds", EWorldMode::Play);
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

        /** One frame: the world, then the mixer (which advances the headless mix). */
        void Frame(const double InDeltaTime = 1.0 / 60.0)
        {
            Worlds.Update(InDeltaTime);
            Worlds.FixedUpdate(InDeltaTime);
            Audio.Update(InDeltaTime);
        }

        Entity MakeSource(const Vector2F& InPosition, const bool bInPlayOnStart, const bool bInSpatial = false)
        {
            Entity lEntity = TheWorld->CreateEntity("Speaker");
            lEntity.Get<TransformComponent>().Position = InPosition;

            AudioSourceComponent lSource;
            lSource.Clip.Path    = OpaaxString("Audio/Beep.wav");
            lSource.bLoop        = true;
            lSource.bPlayOnStart = bInPlayOnStart;
            lSource.bSpatial     = bInSpatial;
            lEntity.AddOrReplace<AudioSourceComponent>(lSource);
            return lEntity;
        }
    };
}

TEST_CASE("AudioSubsystem: a source plays when its entity starts and stops when the entity is destroyed")
{
    AudioWorldFixture lFix;
    Entity lSpeaker = lFix.MakeSource({ 0.f, 0.f }, /*bInPlayOnStart*/true);

    lFix.Frame();
    CHECK(lFix.Sounds->IsSourcePlaying(lSpeaker.GetHandle()));
    CHECK(lFix.Audio.GetVoiceCount() == 1);

    // Considered once: it does not start a second sound.
    lFix.Frame();
    CHECK(lFix.Audio.GetVoiceCount() == 1);

    lFix.TheWorld->DestroyEntity(lSpeaker);
    CHECK(lFix.Audio.GetVoiceCount() == 0);
}

TEST_CASE("AudioSubsystem: a source without bPlayOnStart waits for PlaySource; playing again restarts it")
{
    AudioWorldFixture lFix;
    Entity lSpeaker = lFix.MakeSource({ 0.f, 0.f }, /*bInPlayOnStart*/false);

    lFix.Frame();
    CHECK(lFix.Audio.GetVoiceCount() == 0);

    REQUIRE(lFix.Sounds->PlaySource(lSpeaker.GetHandle()));
    REQUIRE(lFix.Sounds->PlaySource(lSpeaker.GetHandle()));
    CHECK(lFix.Audio.GetVoiceCount() == 1);

    lFix.Sounds->StopSource(lSpeaker.GetHandle());
    CHECK_FALSE(lFix.Sounds->IsSourcePlaying(lSpeaker.GetHandle()));
    CHECK(lFix.Audio.GetVoiceCount() == 0);
}

TEST_CASE("AudioSubsystem: a spatial source follows its entity, heard from the listener entity")
{
    AudioWorldFixture lFix;

    Entity lEar = lFix.TheWorld->CreateEntity("Ear");
    lEar.AddOrReplace<AudioListenerComponent>(AudioListenerComponent{});

    Entity lSpeaker = lFix.MakeSource({ 5000.f, 0.f }, true, /*bInSpatial*/true);
    lFix.Frame();
    CHECK(lFix.Audio.GetListenerPosition().x == doctest::Approx(0.f));
    CHECK(RenderPeaks(lFix.Audio).Max() < 1e-4f);   // beyond the max distance

    lSpeaker.Get<TransformComponent>().Position = { 0.f, 0.f };
    lFix.Frame();
    CHECK(RenderPeaks(lFix.Audio).Max() > 0.1f);

    // The listener follows its entity too. Gain changes are smoothed over a few milliseconds (no
    // click), so one more frame lets the mix settle before it is measured.
    lEar.Get<TransformComponent>().Position = { -5000.f, 0.f };
    lFix.Frame();
    lFix.Frame();
    CHECK(lFix.Audio.GetListenerPosition().x == doctest::Approx(-5000.f));
    CHECK(RenderPeaks(lFix.Audio).Max() < 1e-4f);
}

TEST_CASE("AudioSubsystem: the world's sounds pause with the world")
{
    AudioWorldFixture lFix;
    Entity lSpeaker = lFix.MakeSource({ 0.f, 0.f }, true);
    lFix.Frame();
    REQUIRE(RenderPeaks(lFix.Audio).Max() > 0.1f);

    lFix.Events.GetEventBus().Publish(WorldPauseChanged{ true });
    lFix.Frame();
    CHECK(RenderPeaks(lFix.Audio).Max() < 1e-4f);
    CHECK(lFix.Sounds->IsSourcePlaying(lSpeaker.GetHandle()));   // paused, not ended

    lFix.Events.GetEventBus().Publish(WorldPauseChanged{ false });
    lFix.Frame();
    CHECK(RenderPeaks(lFix.Audio).Max() > 0.1f);
}

TEST_CASE("AudioSubsystem: a behaviour's sound, and every sound of the world, end with the world")
{
    AudioWorldFixture lFix;
    Entity lBell = lFix.TheWorld->CreateEntity("Bell");
    lBell.Add<AudioProbes::Chime>();
    lFix.MakeSource({ 0.f, 0.f }, true);

    lFix.Frame();
    CHECK(lBell.Get<AudioProbes::Chime>().Sound.IsValid());
    CHECK(lFix.Sounds->GetPlayingCount() == 2);
    CHECK(lFix.Audio.GetVoiceCount() == 2);

    // A missing clip plays nothing (logged), and nothing breaks.
    CHECK_FALSE(lFix.Sounds->PlaySound(OpaaxString("Audio/Nope.wav")).IsValid());

    lFix.Worlds.DestroyWorld(lFix.TheWorld);
    lFix.TheWorld = nullptr;
    CHECK(lFix.Audio.GetVoiceCount() == 0);
}
