// Suite: audio — clips (format, decoded or streamed, failure policy), the AudioManager (sounds that
// end, loop, stop and fade, volumes, spatial attenuation and panning, old handles, a full voice pool,
// the audio thread feeding a device) and a Play world's sounds (sources that start with their entity,
// follow it and stop with it; behaviours that play clips). Most of it runs headless: what would be
// heard is rendered into a buffer and measured. The sounds are generated WAV files.
#include <doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

#include "Application/Services/IPaths.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioSubsystem.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Audio/IAudioDevice.h"
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

    AudioClipResource MakeClip(const float InSeconds, const float InAmplitude = 0.5f,
                               const AudioClipResource::EResidency InResidency = AudioClipResource::EResidency::Auto)
    {
        std::optional<AudioClipResource> lClip = AudioClipResource::FromMemory(MakeWav(44100, InSeconds, 440.f, InAmplitude),
                                                                                "Test", InResidency);
        REQUIRE(lClip.has_value());
        return *lClip;
    }

    AudioManager::Desc Headless(const Uint32 InMaxVoices = 16)
    {
        AudioManager::Desc lDesc;
        lDesc.bHeadless  = true;
        lDesc.SampleRate = MIX_RATE;
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

    /** Lets a change (a volume, a stop, a pause) finish its ramp: one mix block. */
    void Settle(AudioManager& InAudio)
    {
        RenderPeaks(InAudio, 256.f / MIX_RATE);
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
TEST_CASE("AudioClip: a WAV file loads with its format and length; a short one is decoded")
{
    const AudioClipResource lClip = MakeClip(0.25f);

    CHECK_FALSE(lClip.IsSilent());
    CHECK(lClip.Channels == 1);
    CHECK(lClip.SampleRate == 44100);
    CHECK(lClip.FrameCount == 11025);
    CHECK(lClip.GetDuration() == doctest::Approx(0.25f));

    CHECK_FALSE(lClip.IsStreamed());
    CHECK(lClip.ByteSize() == 11025 * sizeof(float));
}

TEST_CASE("AudioClip: a long sound keeps its file and is decoded while it plays")
{
    const AudioClipResource lLong = MakeClip(AudioClipResource::DECODE_MAX_SECONDS + 1.f);
    CHECK(lLong.IsStreamed());
    CHECK(lLong.ByteSize() == 44 + lLong.FrameCount * 2);   // the WAV file, not its samples

    const AudioClipResource lForced = MakeClip(0.25f, 0.5f, AudioClipResource::EResidency::Streamed);
    CHECK(lForced.IsStreamed());
    CHECK(lForced.FrameCount == 11025);
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
// AudioManager
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

    // It fades out over one block, then nothing.
    Settle(lAudio);
    CHECK(RenderPeaks(lAudio).Max() == 0.f);
}

TEST_CASE("AudioManager: a stop can fade out; FadeTo and a fade-in take their time")
{
    AudioManager lAudio(Headless());
    REQUIRE(lAudio.Startup());

    PlaySoundParams lParams;
    lParams.bLoop         = true;
    lParams.FadeInSeconds = 0.2f;
    const SoundHandle lSound = lAudio.Play(MakeClip(0.5f), lParams);

    CHECK(RenderPeaks(lAudio, 0.02f).Max() < 0.1f);
    RenderPeaks(lAudio, 0.2f);
    const float lFull = RenderPeaks(lAudio).Max();
    CHECK(lFull == doctest::Approx(0.5f).epsilon(0.05));

    lAudio.FadeTo(lSound, 0.5f, 0.1f);
    RenderPeaks(lAudio, 0.11f);
    CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.5f).epsilon(0.05));

    lAudio.Stop(lSound, 0.5f);
    CHECK_FALSE(lAudio.IsPlaying(lSound));
    CHECK(RenderPeaks(lAudio, 0.2f).Max() > 0.05f);   // still fading
    RenderPeaks(lAudio, 0.35f);
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

    // Each change ramps over one block, so nothing clicks: let it finish, then measure.
    SUBCASE("the bus")
    {
        lAudio.SetBusVolume(EAudioBus::Effects, 0.f);
        Settle(lAudio);
        CHECK(RenderPeaks(lAudio).Max() < 1e-4f);

        // Another bus is not affected.
        lAudio.SetBusVolume(EAudioBus::Effects, 1.f);
        lAudio.SetBusVolume(EAudioBus::Music, 0.f);
        Settle(lAudio);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull).epsilon(0.05));
    }

    SUBCASE("the master")
    {
        lAudio.SetMasterVolume(0.5f);
        Settle(lAudio);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.5f).epsilon(0.05));
    }

    SUBCASE("the sound")
    {
        lAudio.SetVolume(lSound, 0.25f);
        Settle(lAudio);
        CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(lFull * 0.25f).epsilon(0.05));
    }

    SUBCASE("the settings")
    {
        AudioSettings lSettings;
        lSettings.MasterVolume  = 1.f;
        lSettings.EffectsVolume = 0.5f;
        lAudio.ApplySettings(lSettings);
        CHECK(lAudio.GetBusVolume(EAudioBus::Effects) == 0.5f);
        Settle(lAudio);
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

TEST_CASE("AudioManager: a handle of a sound that ended does nothing to the sound that took its voice")
{
    AudioManager lAudio(Headless(/*InMaxVoices*/1));
    REQUIRE(lAudio.Startup());

    const SoundHandle lOld = lAudio.Play(MakeClip(0.05f));
    lAudio.Update(0.1);
    REQUIRE_FALSE(lAudio.IsPlaying(lOld));

    PlaySoundParams lParams;
    lParams.bLoop = true;
    const SoundHandle lNew = lAudio.Play(MakeClip(0.5f), lParams);
    REQUIRE(lNew.GetIndex() == lOld.GetIndex());   // the same voice, another generation
    CHECK(lNew != lOld);

    lAudio.Stop(lOld);
    lAudio.SetVolume(lOld, 0.f);
    lAudio.SetPaused(lOld, true);

    CHECK(lAudio.IsPlaying(lNew));
    Settle(lAudio);
    CHECK(RenderPeaks(lAudio).Max() == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE("AudioManager: a streamed clip sounds exactly like the same clip decoded")
{
    const auto lRender = [](const AudioClipResource& InClip)
    {
        AudioManager lAudio(Headless());
        REQUIRE(lAudio.Startup());
        lAudio.Play(InClip);

        TDynArray<float> lFrames(static_cast<size_t>(MIX_RATE / 2) * 2);
        lAudio.RenderFrames(lFrames.data(), MIX_RATE / 2);
        CHECK(lAudio.GetVoiceCount() == 0);   // ended within the half second
        return lFrames;
    };

    const TDynArray<float> lDecoded  = lRender(MakeClip(0.3f, 0.5f, AudioClipResource::EResidency::Decoded));
    const TDynArray<float> lStreamed = lRender(MakeClip(0.3f, 0.5f, AudioClipResource::EResidency::Streamed));
    CHECK(lDecoded == lStreamed);
    CHECK(*std::max_element(lDecoded.begin(), lDecoded.end()) > 0.4f);
}

TEST_CASE("AudioManager: when every voice plays, a new sound replaces the least important one")
{
    AudioManager lAudio(Headless(/*InMaxVoices*/2));

    // Not started yet.
    CHECK_FALSE(lAudio.Play(MakeClip(0.1f)).IsValid());

    REQUIRE(lAudio.Startup());
    CHECK_FALSE(lAudio.Play(AudioClipResource::Placeholder()).IsValid());

    PlaySoundParams lLoud;
    lLoud.bLoop = true;
    PlaySoundParams lQuiet = lLoud;
    lQuiet.Volume = 0.1f;

    SUBCASE("the quietest of equal priority")
    {
        const SoundHandle lA = lAudio.Play(MakeClip(0.5f), lLoud);
        const SoundHandle lB = lAudio.Play(MakeClip(0.5f), lQuiet);
        CHECK(lAudio.Play(MakeClip(0.5f), lLoud).IsValid());
        CHECK(lAudio.IsPlaying(lA));
        CHECK_FALSE(lAudio.IsPlaying(lB));
        CHECK(lAudio.GetVoiceCount() == 2);
    }

    SUBCASE("the lowest priority, but never for a sound of an even lower one")
    {
        PlaySoundParams lLow = lLoud;
        lLow.Priority = 10;
        PlaySoundParams lLower = lLoud;
        lLower.Priority = 5;

        const SoundHandle lA = lAudio.Play(MakeClip(0.5f), lLow);
        const SoundHandle lB = lAudio.Play(MakeClip(0.5f), lQuiet);   // quieter, but priority 128
        CHECK_FALSE(lAudio.Play(MakeClip(0.5f), lLower).IsValid());
        CHECK(lAudio.Play(MakeClip(0.5f), lLoud).IsValid());
        CHECK_FALSE(lAudio.IsPlaying(lA));
        CHECK(lAudio.IsPlaying(lB));
    }

    // Every voice comes back once the mixer is done with it: two new sounds start in free voices.
    lAudio.StopAll();
    Settle(lAudio);
    const SoundHandle lFirst  = lAudio.Play(MakeClip(0.1f));
    const SoundHandle lSecond = lAudio.Play(MakeClip(0.1f));
    CHECK(lFirst.IsValid());
    CHECK(lSecond.IsValid());
    CHECK(lAudio.GetVoiceCount() == 2);

    lAudio.Shutdown();
    CHECK_FALSE(lAudio.IsStarted());
    CHECK(lAudio.GetVoiceCount() == 0);
}

TEST_CASE("AudioManager: a playing sound keeps its clip's data after the clip is gone")
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

namespace
{
    /** What a recording device heard, shared with the test (the manager owns the device). */
    struct Recording
    {
        Mutex            Lock;
        TDynArray<float> Frames;
        TAtomic<Uint32>  Calls{ 0 };

        /** The loudest sample of the last InFrames frames heard. */
        float LastPeak(const size_t InFrames)
        {
            TLockGuard<Mutex> lGuard(Lock);
            const size_t lStart = Frames.size() > InFrames * 2 ? Frames.size() - InFrames * 2 : 0;

            float lPeak = 0.f;
            for (size_t lIndex = lStart; lIndex < Frames.size(); ++lIndex)
            {
                lPeak = std::max(lPeak, std::abs(Frames[lIndex]));
            }
            return lPeak;
        }
    };

    /** A device with its own thread pulling a period at a time, like a sound card, keeping it all. */
    class RecordingDevice final : public IAudioDevice
    {
    public:
        explicit RecordingDevice(TSharedPtr<Recording> InRecording) : m_Recording(Move(InRecording)) {}
        ~RecordingDevice() override { Stop(); }

        bool Start(const AudioDeviceDesc& InDesc, const AudioDeviceCallback InCallback, void* InUser) override
        {
            m_bRunning = true;
            m_Thread   = Thread([this, InDesc, InCallback, InUser]()
            {
                TDynArray<float> lPeriod(static_cast<size_t>(PERIOD) * InDesc.Channels);
                while (m_bRunning)
                {
                    InCallback(InUser, lPeriod.data(), PERIOD);
                    {
                        TLockGuard<Mutex> lGuard(m_Recording->Lock);
                        m_Recording->Frames.insert(m_Recording->Frames.end(), lPeriod.begin(), lPeriod.end());
                    }
                    ++m_Recording->Calls;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
            return true;
        }

        void Stop() override
        {
            m_bRunning = false;
            if (m_Thread.joinable())
            {
                m_Thread.join();
            }
        }

        Uint32      GetPeriodFrames() const noexcept override { return PERIOD; }
        const char* GetName()         const noexcept override { return "Recording"; }

    private:
        static constexpr Uint32 PERIOD = 256;

        TSharedPtr<Recording> m_Recording;
        Thread                m_Thread;
        TAtomic<bool>         m_bRunning{ false };
    };

    /** Waits up to 5 s for InCondition. */
    bool WaitFor(const TFunction<bool()>& InCondition)
    {
        for (int lTry = 0; lTry < 5000; ++lTry)
        {
            if (InCondition())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return InCondition();
    }
}

TEST_CASE("AudioManager: with a device, the audio thread mixes ahead and the device plays it")
{
    TSharedPtr<Recording> lRecording = MakeShared<Recording>();

    AudioManager::Desc lDesc = Headless();
    lDesc.bHeadless = false;
    AudioManager lAudio(lDesc, MakeUnique<RecordingDevice>(lRecording));
    REQUIRE(lAudio.Startup());
    CHECK_FALSE(lAudio.IsHeadless());

    // Before any sound: the device gets silence.
    REQUIRE(WaitFor([&lRecording]() { return lRecording->Calls.load() > 4; }));
    CHECK(lRecording->LastPeak(1024) == 0.f);

    PlaySoundParams lParams;
    lParams.bLoop = true;
    const SoundHandle lSound = lAudio.Play(MakeClip(0.2f), lParams);
    CHECK(lAudio.IsPlaying(lSound));   // at once, before the audio thread has seen it

    CHECK(WaitFor([&lRecording]() { return lRecording->LastPeak(256) > 0.4f; }));

    lAudio.Stop(lSound);
    CHECK_FALSE(lAudio.IsPlaying(lSound));
    CHECK(WaitFor([&lRecording]() { return lRecording->LastPeak(2048) == 0.f; }));

    lAudio.Shutdown();
    CHECK_FALSE(lAudio.IsStarted());
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

    // The listener follows its entity too. Gain changes ramp over a block (no click), which the
    // frame's mix covers.
    lEar.Get<TransformComponent>().Position = { -5000.f, 0.f };
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
