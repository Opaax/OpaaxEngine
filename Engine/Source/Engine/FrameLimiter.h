#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FrameLimiter — how a frame-rate cap (Window.MaxFrameRate) holds the frames. Told the time, it
    //   says how long to wait before a present; it waits for nothing itself. Pure.
    //
    //   With vsync, a cap that divides the screen's rate (120 or 80 on 240 Hz, 30 on 60 Hz) is held by
    //   the swap interval: each frame is shown for the same whole refreshes (GetHeldRefreshes), with no
    //   wait. Any other cap, or vsync off, is a steady beat of 1 / cap; vsync then shows each frame for
    //   the whole refreshes around it (100 on 240 Hz: 2 or 3). A late frame waits for nothing and the
    //   next counts from it: no burst of short frames to catch up.
    //
    //   A driver may show frames every refresh whatever the swap interval, or a variable-refresh screen
    //   at once: when presents come early UNHELD_FRAMES times in a row, the beat holds the cap instead,
    //   until the cap or vsync changes.
    // =============================================================================
    class FrameLimiter
    {
        // =============================================================================
        // Constants
        // =============================================================================
    public:
        /** How far from a whole number of refreshes a cap's frame may be and still be held for them. */
        static constexpr double WHOLE_TOLERANCE = 0.05;

        /** A held frame presented more than this many refreshes early was not held. */
        static constexpr double EARLY_REFRESHES = 0.5;

        /** Early presents in a row before the beat holds the cap: more than a driver queues. */
        static constexpr Uint32 UNHELD_FRAMES = 8;

        // =============================================================================
        // Settings
        // =============================================================================
    public:
        /** The most frames a second; 0 for no cap. A new cap starts over. */
        void SetMaxFrameRate(Uint32 InFramesPerSecond) noexcept;

        Uint32 GetMaxFrameRate() const noexcept { return m_MaxFrameRate; }

        /** The refresh the presents wait for with vsync, in seconds; 0 without vsync. */
        void SetRefreshInterval(double InInterval) noexcept;

        double GetRefreshInterval() const noexcept { return m_Refresh; }

        /** The next frame waits for nothing and the beat counts from it: after a new pacing. */
        void Restart() noexcept;

        // =============================================================================
        // Per frame
        // =============================================================================
    public:
        /**
         * The wait before this frame's present, at InNow (seconds, on the clock the presents are timed on).
         * @return 0 when it is shown at once: no cap, held by the swap interval, the first frame, a late one
         */
        double WaitBeforePresent(double InNow) noexcept;

        /** The frame's present returned at InEnd: with held refreshes, whether the screen held it. */
        void OnPresented(double InEnd) noexcept;

        // =============================================================================
        // State
        // =============================================================================
    public:
        /**
         * The whole refreshes each frame is shown for, as the swap interval: with vsync and a cap that
         * divides the screen's rate. 0 when the beat holds the cap (or there is none).
         */
        Uint32 GetHeldRefreshes() const noexcept;

        /** True once presents came early with held refreshes: the beat holds the cap until it changes. */
        bool IsRefreshUnheld() const noexcept { return m_bUnheld; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_MaxFrameRate = 0;
        double m_Period       = 0.0;   // 1 / cap, seconds; 0 for none
        double m_Refresh      = 0.0;   // vsync's refresh, seconds; 0 without vsync

        // The beat: when the last frame was let through.
        double m_Due      = 0.0;
        bool   m_bHasBeat = false;

        // Held refreshes: when the last present returned, and the early ones in a row.
        double m_LastPresentEnd = 0.0;
        bool   m_bHasPresent    = false;
        Uint32 m_EarlyPresents  = 0;
        bool   m_bUnheld        = false;
    };
}
