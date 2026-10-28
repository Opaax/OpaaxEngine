#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FrameClock — the time of each frame, the fixed steps it owes, and the alpha the world is drawn
    //   with. A frame longer than MAX_FRAME_DELTA counts as MAX_FRAME_DELTA, so a stall is not
    //   simulated in one go. Restart drops the time since the last frame: the time spent opening a
    //   level is not simulated either. A world that does not tick (paused) is drawn at the alpha it
    //   had, so it holds still. With a screen's refresh, deltas are whole refreshes: the timer's noise
    //   does not reach the frames. Pure: the caller gives the time.
    // =============================================================================
    class FrameClock
    {
    public:
        /** The longest frame simulated, in seconds. */
        static constexpr double MAX_FRAME_DELTA = 0.25;

        /** How far from a whole number of refreshes a delta may be and still be snapped, in refreshes. */
        static constexpr double SNAP_TOLERANCE = 0.45;

        /** The refreshes a stated rate is measured over before the clock snaps to the measured one. */
        static constexpr double MEASURED_REFRESHES = 30.0;

        /** Frames in a row on (off) the screen's rhythm that start (stop) the snapping: a lone stall does not. */
        static constexpr Uint32 RHYTHM_FRAMES = 4;

        /** A step owed to within this fraction of it is taken: sums of one delta land a hair under it. */
        static constexpr double STEP_EPSILON = 1.0e-6;

        explicit FrameClock(double InFixedDelta) noexcept : m_FixedDelta(InFixedDelta) {}

        /**
         * Starts a frame at InNow (seconds): its delta is the time since the last frame or Restart,
         * capped. The first frame has a zero delta.
         * @return The frame's delta
         */
        double BeginFrame(double InNow) noexcept;

        /** True while the frame owes a fixed step; each true takes one. */
        bool TakeFixedStep() noexcept;

        /** The next frame counts from InNow; the fixed steps owed are dropped. */
        void Restart(double InNow) noexcept;

        /**
         * Snaps the deltas to whole refreshes of a screen InInterval seconds apart (0: deltas as
         * measured). A steady screen read by a noisy timer gives noisy deltas, and motion drawn at them
         * shakes; snapped, it does not. The time measured and not given carries to the next frames, so
         * the game's time does not drift. A delta that is no whole number of refreshes (a stall, or
         * frames far shorter than a refresh: vsync not holding) is given as measured, and RHYTHM_FRAMES
         * of them in a row stop the snapping until as many keep to the refresh again.
         * @param bInStated The interval is the screen's stated rate (whole hertz: 60 for 59.94): the clock
         *   measures the real one over the frames that keep to it. False when it is exact (a pacing).
         */
        void SetRefreshInterval(double InInterval, bool bInStated) noexcept;

        /** The refresh the deltas are snapped to (the measured one once known); 0 for none. */
        double GetRefreshInterval() const noexcept;

        /**
         * Steps of InFixedDelta seconds from now on (Time.FixedRate). The time owed is kept: a shorter
         * step owes more of them. A delta that is not above zero is ignored.
         */
        void SetFixedDelta(double InFixedDelta) noexcept;

        /**
         * Whether the world drawn ticks this frame: call once a frame, after Update decided, before the
         * steps. A world that does not (paused) is drawn at the alpha of its last ticked frame, so it
         * holds still; when it ticks again, the time owed starts from that alpha plus this frame's
         * delta, so it goes on from the picture.
         */
        void SetWorldTicks(bool bInTicks) noexcept;

        /** How far into the next fixed step the clock is, from 0 to 1. */
        double GetFixedAlpha() const noexcept { return m_Owed / m_FixedDelta; }

        /** The alpha to draw the world with this frame (after the steps): held while the world does not tick. */
        double GetDisplayAlpha() const noexcept { return m_bHolding ? m_HeldAlpha : GetFixedAlpha(); }

        double GetDelta()      const noexcept { return m_Delta; }
        double GetFixedDelta() const noexcept { return m_FixedDelta; }

    private:
        double m_FixedDelta;
        double m_LastTime = 0.0;
        double m_Delta    = 0.0;
        double m_Owed     = 0.0;   // time the fixed steps have not simulated yet
        bool   m_bStarted = false;

        // The alpha the world was last drawn with while it ticked, and whether it holds there.
        double m_HeldAlpha = 0.0;
        bool   m_bHolding  = false;

        // The screen's refresh; the time measured and not given yet; the measured refresh so far.
        double m_RefreshInterval   = 0.0;
        bool   m_bStatedRefresh    = false;
        double m_Carried           = 0.0;
        double m_MeasuredTime      = 0.0;
        double m_MeasuredRefreshes = 0.0;

        // Frames in a row on and off the screen's rhythm, and whether the deltas are snapped.
        Uint32 m_OnRhythm  = 0;
        Uint32 m_OffRhythm = 0;
        bool   m_bSnapping = false;

        /** InMeasured as whole refreshes, the rest carried; as measured off the screen's rhythm. */
        double SnapToRefresh(double InMeasured) noexcept;
    };
}
