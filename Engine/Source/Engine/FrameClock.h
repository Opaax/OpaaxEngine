#pragma once

namespace Opaax
{
    // =============================================================================
    // FrameClock — the time of each frame, and the fixed steps it owes.
    //   A frame longer than MAX_FRAME_DELTA counts as MAX_FRAME_DELTA, so a stall is not simulated
    //   in one go. Restart drops the time since the last frame: the time spent opening a level is
    //   not simulated either. Pure: the caller gives the time.
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

        /** How far into the next fixed step the clock is, from 0 to 1 (render interpolation). */
        double GetFixedAlpha() const noexcept { return m_Owed / m_FixedDelta; }

        double GetDelta()      const noexcept { return m_Delta; }
        double GetFixedDelta() const noexcept { return m_FixedDelta; }

    private:
        double m_FixedDelta;
        double m_LastTime = 0.0;
        double m_Delta    = 0.0;
        double m_Owed     = 0.0;   // time the fixed steps have not simulated yet
        bool   m_bStarted = false;
    };
}
