// Suite: audio mixing cost — a streamed three-minute music and 30 spatial sounds, mixed block by
// block as the audio thread does. Skipped by default. Run in Release with: build.bat bench
//   -> OpaaxTests.exe --test-suite=perf --no-skip=true
// The budget is the audio design's: under 1 ms per block of 256 frames (5.3 ms of sound).
#include <doctest.h>

#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"

#include "PerfBench.h"
#include "../Audio/AudioTestHelpers.h"

using namespace Opaax;

TEST_SUITE("perf" * doctest::skip())
{
    TEST_CASE("perf: mix a streamed 3-minute music and 30 spatial sounds")
    {
        constexpr Uint32 BLOCK  = 256;
        constexpr Uint32 BLOCKS = 200;

        AudioManager::Desc lDesc;
        lDesc.bHeadless   = true;
        lDesc.SampleRate  = 48000;
        lDesc.MaxVoices   = 64;
        lDesc.BlockFrames = BLOCK;

        AudioManager lAudio(lDesc);
        REQUIRE(lAudio.Startup());

        // The music: decoded while it plays, resampled from 44.1 kHz.
        std::optional<AudioClipResource> lMusic = AudioClipResource::FromMemory(AudioTest::MakeWav(44100, 180.f, 220.f),
                                                                                "Music", AudioClipResource::EResidency::Streamed);
        REQUIRE(lMusic.has_value());

        PlaySoundParams lMusicParams;
        lMusicParams.Bus   = OPAAX_ID("Music");
        lMusicParams.bLoop = true;
        REQUIRE(lAudio.Play(*lMusic, lMusicParams).IsValid());

        // Thirty spatial effects around the listener, each at its own pitch (each resampled).
        std::optional<AudioClipResource> lEffect = AudioClipResource::FromMemory(AudioTest::MakeWav(44100, 0.5f, 660.f), "Effect");
        REQUIRE(lEffect.has_value());
        for (Uint32 lIndex = 0; lIndex < 30; ++lIndex)
        {
            PlaySoundParams lParams;
            lParams.bLoop    = true;
            lParams.bSpatial = true;
            lParams.Pitch    = 0.9f + 0.01f * static_cast<float>(lIndex);
            lParams.Position = Vector2F{ -600.f + 40.f * static_cast<float>(lIndex), 0.f };
            REQUIRE(lAudio.Play(*lEffect, lParams).IsValid());
        }

        TDynArray<float> lOut(static_cast<size_t>(BLOCK) * 2);
        const Perf::Result lResult = Perf::Measure(BLOCKS, 11, [&lAudio, &lOut]
        {
            for (Uint32 lBlock = 0; lBlock < BLOCKS; ++lBlock)
            {
                lAudio.RenderFrames(lOut.data(), BLOCK);
            }
        });

        CHECK(lAudio.GetVoiceCount() == 31);
        Perf::ReportAndGate("audio block (256 frames, 31 voices)", lResult, /*ns/op*/ 1'000'000.0);
    }
}
