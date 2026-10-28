#include "Engine/FrameClock.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    double FrameClock::BeginFrame(const double InNow) noexcept
    {
        // The alpha the last frame was drawn with, while the world ticks: where it holds if it stops.
        if (!m_bHolding)
        {
            m_HeldAlpha = GetFixedAlpha();
        }

        // A clock running backwards (or the first frame) gives a zero delta.
        m_Delta    = m_bStarted ? Maths::Clamp(InNow - m_LastTime, 0.0, MAX_FRAME_DELTA) : 0.0;
        m_LastTime = InNow;
        m_bStarted = true;
        m_Owed    += m_Delta;

        return m_Delta;
    }

    bool FrameClock::TakeFixedStep() noexcept
    {
        if (m_Owed < m_FixedDelta)
        {
            return false;
        }

        m_Owed -= m_FixedDelta;
        return true;
    }

    void FrameClock::Restart(const double InNow) noexcept
    {
        m_LastTime  = InNow;
        m_Owed      = 0.0;
        m_bStarted  = true;
        m_HeldAlpha = 0.0;
    }

    void FrameClock::SetWorldTicks(const bool bInTicks) noexcept
    {
        // Ticking again: on from the picture held, the time spent paused not simulated.
        if (bInTicks && m_bHolding)
        {
            m_Owed = m_HeldAlpha * m_FixedDelta + m_Delta;
        }
        m_bHolding = !bInTicks;
    }

    void FrameClock::SetFixedDelta(const double InFixedDelta) noexcept
    {
        if (InFixedDelta > 0.0)
        {
            m_FixedDelta = InFixedDelta;
        }
    }
}
