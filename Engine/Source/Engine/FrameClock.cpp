#include "Engine/FrameClock.h"

#include <algorithm>

namespace Opaax
{
    double FrameClock::BeginFrame(const double InNow) noexcept
    {
        // A clock running backwards (or the first frame) gives a zero delta.
        m_Delta    = m_bStarted ? std::clamp(InNow - m_LastTime, 0.0, MAX_FRAME_DELTA) : 0.0;
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
        m_LastTime = InNow;
        m_Owed     = 0.0;
        m_bStarted = true;
    }
}
