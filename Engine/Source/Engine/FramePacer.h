#pragma once

#include "Core/Maths/RandomStream.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    // =============================================================================
    // FramePacing — the frames a run is given instead of the platform's clock: a screen's rate, the
    //   noise of the timer that measures it and its stalls, without that screen. What a test uses to
    //   play a level at 144 Hz on any machine, and to get the same frames every run.
    // =============================================================================
    struct FramePacing
    {
        /** The most a frame's time may fall from its tick, as a fraction of a frame (frames stay positive). */
        static constexpr double MAX_JITTER = 0.45;

        /** Frames a second. 0 with no Deltas: off, the platform's clock is used. */
        double Rate = 0.0;

        /**
         * How far each frame's time may fall from its tick of the rate, as a fraction of a frame: 0.25
         * is up to a quarter of a frame early or late. The ticks stay on the rate, so the noise never
         * adds up: a steady screen measured by a noisy timer.
         */
        double Jitter = 0.0;

        /** A stall of HitchSeconds once every HitchEvery seconds. 0: none. */
        double HitchEvery   = 0.0;
        double HitchSeconds = 0.0;

        /** Frame deltas in seconds, repeated in order. Replaces Rate, Jitter and the stalls. */
        TDynArray<double> Deltas;

        /** Seeds the jitter: the same pacing gives the same frames every run. */
        Uint64 Seed = 1;

        bool IsActive() const noexcept { return Rate > 0.0 || !Deltas.empty(); }

        /**
         * Reads a pacing: "144", "144,jitter=0.25", "60,hitch=0.1@2" (a 0.1 s stall every 2 s),
         * "60,seed=3", "deltas=0.016/0.033". "off" or nothing is no pacing.
         * @return False, with OutError saying what is wrong, when InText is not a pacing
         */
        static bool Parse(OpaaxStringView InText, FramePacing& OutPacing, OpaaxString& OutError);

        /** The pacing written as Parse reads it ("off" when inactive). */
        OpaaxString ToString() const;
    };

    // =============================================================================
    // FramePacer — the frame deltas of a pacing, one per call. Pure.
    // =============================================================================
    class FramePacer
    {
    public:
        explicit FramePacer(const FramePacing& InPacing = FramePacing{});

        /** The next frame's delta in seconds, always positive. 0 when the pacing is off. */
        double Next();

        const FramePacing& GetPacing() const noexcept { return m_Pacing; }

    private:
        FramePacing  m_Pacing;
        RandomStream m_Random;

        Uint64 m_Frame     = 0;
        double m_Offset    = 0.0;   // how far the last frame fell from its tick, seconds
        double m_Elapsed   = 0.0;   // the ticks so far, seconds: where the stalls fall
        double m_NextHitch = 0.0;
    };
}
