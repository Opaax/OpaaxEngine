#include "Engine/FrameLimiter.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    // =============================================================================
    // Settings
    // =============================================================================
    void FrameLimiter::SetMaxFrameRate(const Uint32 InFramesPerSecond) noexcept
    {
        if (InFramesPerSecond == m_MaxFrameRate)
        {
            return;
        }

        m_MaxFrameRate = InFramesPerSecond;
        m_Period       = InFramesPerSecond > 0 ? 1.0 / static_cast<double>(InFramesPerSecond) : 0.0;

        // A new cap tries the swap interval again.
        m_bUnheld = false;
        Restart();
    }

    void FrameLimiter::SetRefreshInterval(const double InInterval) noexcept
    {
        const double lInterval = Maths::Max(InInterval, 0.0);

        // Vsync turned on tries the swap interval again.
        if (m_Refresh <= 0.0 && lInterval > 0.0)
        {
            m_bUnheld       = false;
            m_EarlyPresents = 0;
        }

        m_Refresh = lInterval;
    }

    void FrameLimiter::Restart() noexcept
    {
        m_bHasBeat      = false;
        m_bHasPresent   = false;
        m_EarlyPresents = 0;
    }

    // =============================================================================
    // Per frame
    // =============================================================================
    double FrameLimiter::WaitBeforePresent(const double InNow) noexcept
    {
        if (m_Period <= 0.0 || GetHeldRefreshes() > 0)
        {
            return 0.0;
        }

        // A period after the last frame let through; a late frame starts the beat again from itself.
        const double lNext = m_Due + m_Period;
        if (!m_bHasBeat || InNow >= lNext)
        {
            m_Due      = InNow;
            m_bHasBeat = true;
            return 0.0;
        }

        m_Due = lNext;
        return lNext - InNow;
    }

    void FrameLimiter::OnPresented(const double InEnd) noexcept
    {
        // A held frame comes whole refreshes after the last one, or later when it is late: one that
        // came early was shown at the next refresh.
        if (const Uint32 lHeld = GetHeldRefreshes(); lHeld > 0 && m_bHasPresent)
        {
            const double lEarliest = m_LastPresentEnd + (static_cast<double>(lHeld) - EARLY_REFRESHES) * m_Refresh;

            m_EarlyPresents = InEnd < lEarliest ? m_EarlyPresents + 1 : 0;
            if (m_EarlyPresents >= UNHELD_FRAMES)
            {
                m_bUnheld       = true;
                m_EarlyPresents = 0;
            }
        }

        m_LastPresentEnd = InEnd;
        m_bHasPresent    = true;
    }

    // =============================================================================
    // State
    // =============================================================================
    Uint32 FrameLimiter::GetHeldRefreshes() const noexcept
    {
        if (m_Period <= 0.0 || m_Refresh <= 0.0 || m_bUnheld)
        {
            return 0;
        }

        // A cap at or over the screen's rate is the beat: vsync alone keeps the frames under it, and
        // the beat holds them where it does not.
        const double lRefreshes = m_Period / m_Refresh;
        const double lWhole     = Maths::RoundHalfFromZero(lRefreshes);
        if (lWhole < 2.0 || Maths::Abs(lRefreshes - lWhole) > WHOLE_TOLERANCE)
        {
            return 0;
        }

        return static_cast<Uint32>(lWhole);
    }
}
