#pragma once

namespace Opaax
{
    // =============================================================================
    // FrameClock — the time of each frame, the fixed steps it owes, and the alpha the world is drawn
    //   with. A frame longer than MAX_FRAME_DELTA counts as MAX_FRAME_DELTA, so a stall is not
    //   simulated in one go. Restart drops the time since the last frame: the time spent opening a
    //   level is not simulated either. A world that does not tick (paused) is drawn at the alpha it
    //   had, so it holds still. Pure: the caller gives the time.
    // =============================================================================
    class FrameClock
    {
    public:
        /** The longest frame simulated, in seconds. */
        static constexpr double MAX_FRAME_DELTA = 0.25;

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
    };
}
