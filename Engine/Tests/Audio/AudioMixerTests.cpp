// Suite: the engine's own mixer, without an audio library or a thread — the lock-free queues, the
// fader, distance and pan, and the mixer itself: voices addressed by handle (an old handle does
// nothing), the bus tree (volume, mute, solo), fades, stops without a click, pitch and sample rates,
// pause, voices replaced while heard, and streamed sources. The mix is rendered into a buffer and
// measured.
#include <doctest.h>

#include "Core/Memory/Memory.h"
#include "Core/Maths/Maths.h"

#include "Audio/AudioTypes.h"
#include "Audio/IAudioDecoder.h"
#include "Audio/Mixer/AudioFader.h"
#include "Audio/Mixer/AudioMixer.h"
#include "Audio/Mixer/AudioRingBuffer.h"
#include "Audio/Mixer/AudioSpatial.h"
#include "Audio/Mixer/AudioStreamReader.h"
#include "Audio/Mixer/SpscQueue.h"

using namespace Opaax;

namespace
{
    constexpr Uint32 MIX_RATE  = 48000;
    constexpr Uint32 CLIP_RATE = 44100;

    /** InSeconds of a sine at InHz, peaking at InAmplitude; stereo has the same wave on both sides. */
    TDynArray<float> Sine(const float InSeconds, const float InAmplitude = 0.5f, const Uint32 InChannels = 1,
                          const Uint32 InRate = CLIP_RATE, const float InHz = 440.f)
    {
        const Uint32 lFrames = static_cast<Uint32>(InSeconds * static_cast<float>(InRate));
        TDynArray<float> lOut;
        lOut.reserve(static_cast<size_t>(lFrames) * InChannels);

        for (Uint32 lFrame = 0; lFrame < lFrames; ++lFrame)
        {
            const float lSample = InAmplitude * static_cast<float>(Maths::Sin(2.0 * 3.14159265358979 * InHz * lFrame / InRate));
            for (Uint32 lChannel = 0; lChannel < InChannels; ++lChannel)
            {
                lOut.push_back(lSample);
            }
        }
        return lOut;
    }

    /** A decoder over frames in memory, reading at most InMaxRead frames per call. */
    class MemoryDecoder final : public IAudioDecoder
    {
    public:
        MemoryDecoder(const TDynArray<float>& InFrames, const Uint32 InChannels, const Uint32 InRate, const Uint64 InMaxRead)
            : m_Frames(InFrames)
            , m_MaxRead(InMaxRead)
        {
            m_Format.Channels   = InChannels;
            m_Format.SampleRate = InRate;
            m_Format.FrameCount = InFrames.size() / InChannels;
        }

        const AudioFormat& GetFormat() const noexcept override { return m_Format; }

        Uint64 Read(float* OutFrames, const Uint64 InFrameCount) override
        {
            const Uint64 lCount = Maths::Min({ InFrameCount, m_MaxRead, m_Format.FrameCount - m_Position });
            Memory::Copy(OutFrames, m_Frames.data() + m_Position * m_Format.Channels,
                         sizeof(float) * lCount * m_Format.Channels);
            m_Position += lCount;
            return lCount;
        }

        bool Rewind() override
        {
            m_Position = 0;
            return true;
        }

    private:
        TDynArray<float> m_Frames;
        AudioFormat      m_Format;
        Uint64           m_MaxRead  = 0;
        Uint64           m_Position = 0;
    };

    struct Peaks
    {
        float Left  = 0.f;
        float Right = 0.f;
        float Jump  = 0.f;   // largest change between two frames on one side: a click when large

        float Max() const { return Maths::Max(Left, Right); }
    };

    AudioMixer::Desc ThreeBuses()
    {
        AudioMixer::Desc lDesc;
        lDesc.SampleRate = MIX_RATE;
        lDesc.MaxVoices  = 8;
        lDesc.Buses      = { { OPAAX_ID("Master"), -1, 1.f }, { OPAAX_ID("Music"), 0, 1.f }, { OPAAX_ID("SFX"), 0, 1.f } };
        return lDesc;
    }

    constexpr Uint16 MASTER = 0;
    constexpr Uint16 MUSIC  = 1;
    constexpr Uint16 SFX    = 2;

    /** A mixer, its event queue, and helpers to command it and measure what it mixes. */
    struct MixerRig
    {
        TSpscQueue<AudioEvent> Events{ 64 };
        AudioMixer             Mixer;
        Uint32                 NextGeneration = 1;
        float                  LastLeft       = 0.f;   // the last frame rendered
        float                  LastRight      = 0.f;

        explicit MixerRig(const AudioMixer::Desc& InDesc = ThreeBuses())
            : Mixer(InDesc, Events)
        {
        }

        static AudioVoiceDesc Voice(const TDynArray<float>& InFrames, const Uint32 InChannels = 1, const Uint32 InRate = CLIP_RATE)
        {
            AudioVoiceDesc lVoice;
            lVoice.Source.Frames     = InFrames.data();
            lVoice.Source.FrameCount = InFrames.size() / InChannels;
            lVoice.Source.Channels   = InChannels;
            lVoice.Source.SampleRate = InRate;
            lVoice.Bus               = SFX;
            return lVoice;
        }

        SoundHandle Play(const Uint32 InIndex, const AudioVoiceDesc& InVoice)
        {
            AudioCommand lCommand;
            lCommand.Type   = EAudioCommand::Play;
            lCommand.Handle = SoundHandle::Make(InIndex, NextGeneration++);
            lCommand.Voice  = InVoice;
            Mixer.Apply(lCommand);
            return lCommand.Handle;
        }

        void Send(const EAudioCommand InType, const SoundHandle InHandle, const float InValue = 0.f, const Uint32 InFrames = 0,
                  const bool bInFlag = false)
        {
            AudioCommand lCommand;
            lCommand.Type   = InType;
            lCommand.Handle = InHandle;
            lCommand.Value  = InValue;
            lCommand.Frames = InFrames;
            lCommand.bFlag  = bInFlag;
            Mixer.Apply(lCommand);
        }

        void SendBus(const EAudioCommand InType, const Uint16 InBus, const float InValue = 0.f, const Uint32 InFrames = 0,
                     const bool bInFlag = false)
        {
            AudioCommand lCommand;
            lCommand.Type   = InType;
            lCommand.Bus    = InBus;
            lCommand.Value  = InValue;
            lCommand.Frames = InFrames;
            lCommand.bFlag  = bInFlag;
            Mixer.Apply(lCommand);
        }

        void SendPosition(const EAudioCommand InType, const SoundHandle InHandle, const Vector2F InPosition)
        {
            AudioCommand lCommand;
            lCommand.Type     = InType;
            lCommand.Handle   = InHandle;
            lCommand.Position = InPosition;
            Mixer.Apply(lCommand);
        }

        Peaks Render(const float InSeconds)
        {
            return RenderFrames(static_cast<Uint32>(Maths::RoundHalfFromZero(InSeconds * MIX_RATE)));
        }

        Peaks RenderFrames(const Uint32 InFrames)
        {
            TDynArray<float> lBuffer(static_cast<size_t>(InFrames) * AUDIO_OUTPUT_CHANNELS, -1.f);
            Mixer.Mix(lBuffer.data(), InFrames);

            // The jump is measured from the last frame of the previous render: a change applied
            // between two renders clicks right at the start of the second.
            Peaks lPeaks;
            for (Uint32 lFrame = 0; lFrame < InFrames; ++lFrame)
            {
                const float lLeft  = lBuffer[lFrame * 2];
                const float lRight = lBuffer[lFrame * 2 + 1];
                lPeaks.Left  = Maths::Max(lPeaks.Left, Maths::Abs(lLeft));
                lPeaks.Right = Maths::Max(lPeaks.Right, Maths::Abs(lRight));
                lPeaks.Jump  = Maths::Max({ lPeaks.Jump, Maths::Abs(lLeft - LastLeft), Maths::Abs(lRight - LastRight) });

                LastLeft  = lLeft;
                LastRight = lRight;
            }
            return lPeaks;
        }

        TDynArray<SoundHandle> Ended()
        {
            TDynArray<SoundHandle> lEnded;
            AudioEvent lEvent;
            while (Events.Pop(lEvent))
            {
                lEnded.push_back(lEvent.Handle);
            }
            return lEnded;
        }
    };

    /** Largest change between two frames of a 0.5 sine at 440 Hz, mixed at 48 kHz: what is not a click. */
    constexpr float SMOOTH_JUMP = 0.035f;

    /**
     * A time where the 440 Hz sine is near its peak (44 cycles and a quarter): a change made there
     * clicks if it is not ramped. Whole cycles would end on a zero crossing, where a cut is silent.
     */
    constexpr float NEAR_PEAK = 0.1006f;
}

// =============================================================================
// Handles and queues
// =============================================================================
TEST_CASE("SoundHandle: an index and a generation; no generation is no sound")
{
    const SoundHandle lHandle = SoundHandle::Make(7, 42);
    CHECK(lHandle.IsValid());
    CHECK(lHandle.GetIndex() == 7);
    CHECK(lHandle.GetGeneration() == 42);
    CHECK_FALSE(SoundHandle{}.IsValid());
    CHECK(SoundHandle::Make(7, 43) != lHandle);
}

TEST_CASE("TSpscQueue: items come out in order; a full queue refuses; two threads share it")
{
    TSpscQueue<Uint32> lQueue(4);
    CHECK(lQueue.Capacity() >= 4);
    CHECK(lQueue.IsEmpty());

    for (Uint32 lValue = 0; lValue < lQueue.Capacity(); ++lValue)
    {
        CHECK(lQueue.Push(lValue));
    }
    CHECK_FALSE(lQueue.Push(99));

    Uint32 lOut = 0;
    for (Uint32 lValue = 0; lValue < lQueue.Capacity(); ++lValue)
    {
        REQUIRE(lQueue.Pop(lOut));
        CHECK(lOut == lValue);
    }
    CHECK_FALSE(lQueue.Pop(lOut));

    // One producer, one consumer: nothing lost, nothing reordered.
    constexpr Uint32 COUNT = 200000;
    TSpscQueue<Uint32> lShared(64);
    Thread lProducer([&lShared]()
    {
        for (Uint32 lValue = 0; lValue < COUNT; ++lValue)
        {
            while (!lShared.Push(lValue)) {}
        }
    });

    Uint32 lExpected = 0;
    bool   bInOrder  = true;
    while (lExpected < COUNT)
    {
        if (lShared.Pop(lOut))
        {
            bInOrder = bInOrder && lOut == lExpected;
            ++lExpected;
        }
    }
    lProducer.join();
    CHECK(bInOrder);
}

TEST_CASE("AudioRingBuffer: frames come out in order across the wrap; reading ahead of the writer gives less")
{
    AudioRingBuffer lRing(8, 2);
    CHECK(lRing.GetFreeFrames() == 8);

    TDynArray<float> lFrames;
    for (int lValue = 0; lValue < 24; ++lValue) { lFrames.push_back(static_cast<float>(lValue)); }

    CHECK(lRing.Write(lFrames.data(), 6) == 6);
    TDynArray<float> lOut(16, -1.f);
    CHECK(lRing.Read(lOut.data(), 4) == 4);
    CHECK(lOut[0] == 0.f);
    CHECK(lOut[7] == 7.f);

    // 2 waiting + 6 more: this write wraps around the end.
    CHECK(lRing.Write(lFrames.data() + 12, 8) == 6);
    CHECK(lRing.GetFreeFrames() == 0);
    CHECK(lRing.Read(lOut.data(), 8) == 8);
    CHECK(lOut[0] == 8.f);    // frame 4
    CHECK(lOut[3] == 11.f);   // frame 5
    CHECK(lOut[4] == 12.f);   // the first frame of the second write
    CHECK(lOut[15] == 23.f);

    CHECK(lRing.Read(lOut.data(), 4) == 0);
}

// =============================================================================
// Fader, distance and pan
// =============================================================================
TEST_CASE("AudioFader: reaches its target in the frames asked, in a straight line")
{
    AudioFader lFader;
    lFader.Set(0.f);
    lFader.FadeTo(1.f, 100);

    lFader.Tick(25);
    CHECK(lFader.Current == doctest::Approx(0.25f));
    CHECK(lFader.IsFading());

    lFader.Tick(50);
    CHECK(lFader.Current == doctest::Approx(0.75f));

    lFader.Tick(500);
    CHECK(lFader.Current == 1.f);
    CHECK_FALSE(lFader.IsFading());

    lFader.FadeTo(0.f, 0);
    CHECK(lFader.IsSilent());
}

TEST_CASE("AudioSpatial: full within the min distance, silent from the max on, panned by the X offset")
{
    using namespace AudioSpatial;

    CHECK(Attenuate(EAudioAttenuation::Linear, 50.f, 100.f, 1100.f) == 1.f);
    CHECK(Attenuate(EAudioAttenuation::Linear, 600.f, 100.f, 1100.f) == doctest::Approx(0.5f));
    CHECK(Attenuate(EAudioAttenuation::Linear, 2000.f, 100.f, 1100.f) == 0.f);

    // Inverse square: the same ends, quieter than linear in between, and always going down.
    CHECK(Attenuate(EAudioAttenuation::InverseSquare, 100.f, 100.f, 1100.f) == doctest::Approx(1.f));
    CHECK(Attenuate(EAudioAttenuation::InverseSquare, 1100.f, 100.f, 1100.f) == 0.f);
    CHECK(Attenuate(EAudioAttenuation::InverseSquare, 600.f, 100.f, 1100.f) < 0.5f);
    CHECK(Attenuate(EAudioAttenuation::InverseSquare, 300.f, 100.f, 1100.f) > Attenuate(EAudioAttenuation::InverseSquare, 400.f, 100.f, 1100.f));

    CHECK(PanFromOffset(500.f, 1000.f) == doctest::Approx(0.5f));
    CHECK(PanFromOffset(-5000.f, 1000.f) == -1.f);

    const Vector2F lCentre = BalanceGains(0.f);
    CHECK(lCentre.x == 1.f);
    CHECK(lCentre.y == 1.f);

    const Vector2F lRight = BalanceGains(0.5f);
    CHECK(lRight.x == doctest::Approx(0.5f));
    CHECK(lRight.y == 1.f);
}

// =============================================================================
// Mixer
// =============================================================================
TEST_CASE("AudioMixer: a voice plays its source to the end, resampled, then reports it ended")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.25f);
    const SoundHandle lSound = lRig.Play(3, MixerRig::Voice(lClip));
    CHECK(lRig.Mixer.GetActiveVoiceCount() == 1);

    const Peaks lPeaks = lRig.Render(0.2f);
    CHECK(lPeaks.Left == doctest::Approx(0.5f).epsilon(0.02));   // mono: the same on both sides
    CHECK(lPeaks.Right == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(lPeaks.Jump < SMOOTH_JUMP);
    CHECK(lRig.Ended().empty());

    lRig.Render(0.1f);
    const TDynArray<SoundHandle> lEnded = lRig.Ended();
    REQUIRE(lEnded.size() == 1);
    CHECK(lEnded[0] == lSound);
    CHECK(lRig.Mixer.GetActiveVoiceCount() == 0);
    CHECK(lRig.Render(0.05f).Max() == 0.f);
}

TEST_CASE("AudioMixer: a stop fades out over one block without a click; a fade-out stop takes its time")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.5f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.bLoop = true;

    SUBCASE("at once")
    {
        const SoundHandle lSound = lRig.Play(0, lVoice);
        lRig.Render(NEAR_PEAK);

        lRig.Send(EAudioCommand::Stop, lSound);
        const Peaks lStop = lRig.RenderFrames(lRig.Mixer.GetBlockFrames());
        CHECK(lStop.Jump < SMOOTH_JUMP);
        CHECK(lRig.Ended().size() == 1);
        CHECK(lRig.Render(0.05f).Max() == 0.f);
    }

    SUBCASE("over 0.2 s")
    {
        const SoundHandle lSound = lRig.Play(0, lVoice);
        lRig.Send(EAudioCommand::Stop, lSound, 0.f, MIX_RATE / 5);

        const Peaks lHalf = lRig.Render(0.1f);
        CHECK(lHalf.Jump < SMOOTH_JUMP);
        CHECK(lRig.Ended().empty());

        lRig.Render(0.11f);
        CHECK(lRig.Ended().size() == 1);
    }
}

TEST_CASE("AudioMixer: a command for an old handle does nothing to the sound that took its voice")
{
    MixerRig lRig;
    const TDynArray<float> lShort = Sine(0.05f);
    const TDynArray<float> lLong  = Sine(0.5f);

    const SoundHandle lOld = lRig.Play(0, MixerRig::Voice(lShort));
    lRig.Render(0.1f);
    REQUIRE(lRig.Ended().size() == 1);

    AudioVoiceDesc lVoice = MixerRig::Voice(lLong);
    lVoice.bLoop = true;
    const SoundHandle lNew = lRig.Play(0, lVoice);
    REQUIRE(lNew.GetIndex() == lOld.GetIndex());

    lRig.Send(EAudioCommand::Stop, lOld);
    lRig.Send(EAudioCommand::SetVolume, lOld, 0.f);
    lRig.Send(EAudioCommand::SetPaused, lOld, 0.f, 0, true);

    CHECK(lRig.Render(0.05f).Max() == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(lRig.Ended().empty());
}

TEST_CASE("AudioMixer: what is heard is the voice volume times its bus chain; mute and solo")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.5f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.bLoop = true;
    const SoundHandle lSound = lRig.Play(0, lVoice);

    const float lFull = lRig.Render(0.05f).Max();
    REQUIRE(lFull == doctest::Approx(0.5f).epsilon(0.02));

    // Each change ramps over one block: render one, then measure.
    const auto lSettle = [&lRig]() { lRig.RenderFrames(lRig.Mixer.GetBlockFrames()); return lRig.Render(0.05f).Max(); };

    SUBCASE("the voice")
    {
        lRig.Send(EAudioCommand::SetVolume, lSound, 0.5f);
        CHECK(lSettle() == doctest::Approx(lFull * 0.5f).epsilon(0.02));
    }

    SUBCASE("its bus and the master")
    {
        lRig.SendBus(EAudioCommand::SetBusVolume, SFX, 0.5f);
        lRig.SendBus(EAudioCommand::SetBusVolume, MASTER, 0.5f);
        CHECK(lSettle() == doctest::Approx(lFull * 0.25f).epsilon(0.02));
    }

    SUBCASE("another bus")
    {
        lRig.SendBus(EAudioCommand::SetBusVolume, MUSIC, 0.f);
        CHECK(lSettle() == doctest::Approx(lFull).epsilon(0.02));
    }

    SUBCASE("mute")
    {
        lRig.Render(NEAR_PEAK);
        lRig.SendBus(EAudioCommand::SetBusMute, MASTER, 0.f, 0, true);
        CHECK(lRig.RenderFrames(lRig.Mixer.GetBlockFrames()).Jump < SMOOTH_JUMP);
        CHECK(lSettle() == 0.f);
        lRig.SendBus(EAudioCommand::SetBusMute, MASTER, 0.f, 0, false);
        CHECK(lSettle() == doctest::Approx(lFull).epsilon(0.02));
    }

    SUBCASE("solo")
    {
        lRig.SendBus(EAudioCommand::SetBusSolo, MUSIC, 0.f, 0, true);
        CHECK(lSettle() == 0.f);
        lRig.SendBus(EAudioCommand::SetBusSolo, SFX, 0.f, 0, true);
        CHECK(lSettle() == doctest::Approx(lFull).epsilon(0.02));
    }
}

TEST_CASE("AudioMixer: a fade-in starts silent; FadeVolume and a bus fade reach their target in time")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(1.f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.bLoop        = true;
    lVoice.FadeInFrames = MIX_RATE / 10;
    const SoundHandle lSound = lRig.Play(0, lVoice);

    const Peaks lStart = lRig.Render(0.01f);
    CHECK(lStart.Max() < 0.1f);
    lRig.Render(0.1f);
    CHECK(lRig.Render(0.05f).Max() == doctest::Approx(0.5f).epsilon(0.02));

    lRig.Send(EAudioCommand::FadeVolume, lSound, 0.5f, MIX_RATE / 10);
    const Peaks lFading = lRig.Render(0.11f);
    CHECK(lFading.Jump < SMOOTH_JUMP);
    CHECK(lRig.Render(0.05f).Max() == doctest::Approx(0.25f).epsilon(0.02));

    lRig.SendBus(EAudioCommand::FadeBus, SFX, 0.f, MIX_RATE / 10);
    lRig.Render(0.11f);
    CHECK(lRig.Render(0.05f).Max() == 0.f);
    CHECK(lRig.Ended().empty());   // a silent bus does not end its sounds
}

TEST_CASE("AudioMixer: a spatial voice is panned by its X offset and silent beyond its max distance")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.5f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.bLoop       = true;
    lVoice.bSpatial    = true;
    lVoice.MinDistance = 100.f;
    lVoice.MaxDistance = 1000.f;

    SUBCASE("to the right")
    {
        lVoice.Position = { 300.f, 0.f };
        lRig.Play(0, lVoice);
        const Peaks lPeaks = lRig.Render(0.05f);
        CHECK(lPeaks.Right > 0.1f);
        CHECK(lPeaks.Left < lPeaks.Right);
    }

    SUBCASE("to the left")
    {
        lVoice.Position = { -300.f, 0.f };
        lRig.Play(0, lVoice);
        const Peaks lPeaks = lRig.Render(0.05f);
        CHECK(lPeaks.Left > lPeaks.Right);
    }

    SUBCASE("too far, then the listener comes, then the sound leaves")
    {
        lVoice.Position = { 5000.f, 0.f };
        const SoundHandle lSound = lRig.Play(0, lVoice);
        CHECK(lRig.Render(NEAR_PEAK).Max() == 0.f);

        lRig.SendPosition(EAudioCommand::SetListener, SoundHandle{}, { 5000.f, 0.f });
        const Peaks lNear = lRig.Render(0.05f);
        CHECK(lNear.Max() > 0.4f);
        CHECK(lNear.Jump < SMOOTH_JUMP);

        lRig.SendPosition(EAudioCommand::SetPosition, lSound, { 0.f, 0.f });
        lRig.Render(0.01f);
        CHECK(lRig.Render(0.05f).Max() == 0.f);
    }
}

TEST_CASE("AudioMixer: pitch 2 plays a sound in half its length; a stereo source keeps its sides")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.4f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.Pitch = 2.f;
    lRig.Play(0, lVoice);

    lRig.Render(0.18f);
    CHECK(lRig.Ended().empty());
    lRig.Render(0.04f);
    CHECK(lRig.Ended().size() == 1);

    // Left only: the right side stays silent.
    TDynArray<float> lStereo = Sine(0.2f, 0.5f, 2, MIX_RATE);
    for (size_t lIndex = 1; lIndex < lStereo.size(); lIndex += 2) { lStereo[lIndex] = 0.f; }
    lRig.Play(1, MixerRig::Voice(lStereo, 2, MIX_RATE));

    const Peaks lPeaks = lRig.Render(0.1f);
    CHECK(lPeaks.Left == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(lPeaks.Right == 0.f);
}

TEST_CASE("AudioMixer: a paused voice holds its place and goes on from it")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.2f);
    const SoundHandle lSound = lRig.Play(0, MixerRig::Voice(lClip));

    lRig.Render(NEAR_PEAK);
    lRig.Send(EAudioCommand::SetPaused, lSound, 0.f, 0, true);
    CHECK(lRig.Render(0.01f).Jump < SMOOTH_JUMP);
    CHECK(lRig.Render(1.f).Max() == 0.f);
    CHECK(lRig.Ended().empty());

    lRig.Send(EAudioCommand::SetPaused, lSound, 0.f, 0, false);
    CHECK(lRig.Render(0.05f).Max() > 0.4f);
    CHECK(lRig.Ended().empty());

    lRig.Render(0.06f);
    CHECK(lRig.Ended().size() == 1);
}

TEST_CASE("AudioMixer: a voice replaced while heard fades out apart and reports its end; nothing to play ends at once")
{
    MixerRig lRig;
    const TDynArray<float> lClip = Sine(0.5f);
    AudioVoiceDesc lVoice = MixerRig::Voice(lClip);
    lVoice.bLoop = true;

    const SoundHandle lFirst = lRig.Play(0, lVoice);
    lRig.Render(NEAR_PEAK);

    const SoundHandle lSecond = lRig.Play(0, lVoice);
    // For one block the first fades out under the second: two sines, and no cut.
    const Peaks lSwap = lRig.Render(0.05f);
    CHECK(lSwap.Jump < 2.f * SMOOTH_JUMP);
    const TDynArray<SoundHandle> lEnded = lRig.Ended();
    REQUIRE(lEnded.size() == 1);
    CHECK(lEnded[0] == lFirst);
    CHECK(lRig.Mixer.GetActiveVoiceCount() == 1);

    lRig.Send(EAudioCommand::Stop, lSecond);
    lRig.Render(0.01f);
    CHECK(lRig.Ended().size() == 1);

    // An empty source: the voice ends without being mixed.
    const SoundHandle lEmpty = lRig.Play(1, AudioVoiceDesc{});
    const TDynArray<SoundHandle> lAtOnce = lRig.Ended();
    REQUIRE(lAtOnce.size() == 1);
    CHECK(lAtOnce[0] == lEmpty);
    CHECK(lRig.Mixer.GetActiveVoiceCount() == 0);
}

TEST_CASE("AudioStreamReader: a streamed voice sounds exactly like the same clip decoded, and loops")
{
    const TDynArray<float> lClip = Sine(0.3f, 0.5f, 2);
    const Uint32 lWindow = AudioMixer::GetStreamWindowFrames(256);

    SUBCASE("once")
    {
        MixerRig lDecoded;
        MixerRig lStreamed;
        AudioStreamReader lReader(MakeUnique<MemoryDecoder>(lClip, 2, CLIP_RATE, 100), lWindow);

        AudioVoiceDesc lFromMemory = MixerRig::Voice(lClip, 2);
        AudioVoiceDesc lFromStream = lFromMemory;
        lFromStream.Source.Frames = nullptr;
        lFromStream.Source.Stream = &lReader;

        lDecoded.Play(0, lFromMemory);
        lStreamed.Play(0, lFromStream);

        const Uint32 lFrames = MIX_RATE / 2;
        TDynArray<float> lA(lFrames * 2), lB(lFrames * 2);
        lDecoded.Mixer.Mix(lA.data(), lFrames);
        lStreamed.Mixer.Mix(lB.data(), lFrames);

        CHECK(lA == lB);
        CHECK(lDecoded.Ended().size() == 1);
        CHECK(lStreamed.Ended().size() == 1);
    }

    SUBCASE("looping")
    {
        MixerRig lRig;
        AudioStreamReader lReader(MakeUnique<MemoryDecoder>(lClip, 2, CLIP_RATE, 100), lWindow);

        AudioVoiceDesc lVoice = MixerRig::Voice(lClip, 2);
        lVoice.Source.Frames = nullptr;
        lVoice.Source.Stream = &lReader;
        lVoice.bLoop         = true;
        lRig.Play(0, lVoice);

        lRig.Render(1.f);
        const Peaks lLater = lRig.Render(0.05f);
        CHECK(lLater.Max() == doctest::Approx(0.5f).epsilon(0.02));
        CHECK(lRig.Ended().empty());
    }
}
