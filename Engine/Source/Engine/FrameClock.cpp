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
        const double lMeasured = m_bStarted ? Maths::Clamp(InNow - m_LastTime, 0.0, MAX_FRAME_DELTA) : 0.0;
        m_LastTime = InNow;
        m_bStarted = true;
        m_Delta    = SnapToRefresh(lMeasured);
        m_Owed    += m_Delta;

        m_StepsThisFrame = 0;

        return m_Delta;
    }

    bool FrameClock::TakeFixedStep() noexcept
    {
        if (m_Owed < m_FixedDelta * (1.0 - STEP_EPSILON))
        {
            return false;
        }

        // Past the frame's most: the whole steps left are dropped, the part of one kept.
        if (m_StepsThisFrame >= m_MaxSteps)
        {
            const Int64 lDropped = Maths::FloorToInt64(m_Owed / m_FixedDelta + STEP_EPSILON);
            m_DroppedSteps += static_cast<Uint64>(lDropped);
            m_Owed          = Maths::Max(m_Owed - static_cast<double>(lDropped) * m_FixedDelta, 0.0);
            return false;
        }

        ++m_StepsThisFrame;
        m_Owed = Maths::Max(m_Owed - m_FixedDelta, 0.0);
        return true;
    }

    void FrameClock::Restart(const double InNow) noexcept
    {
        m_LastTime  = InNow;
        m_Owed      = 0.0;
        m_bStarted  = true;
        m_HeldAlpha = 0.0;
        m_Carried   = 0.0;
    }

    void FrameClock::SetMaxStepsPerFrame(const Uint32 InMax) noexcept
    {
        m_MaxSteps = Maths::Max(InMax, 1u);
    }

    void FrameClock::SetRefreshInterval(const double InInterval, const bool bInStated) noexcept
    {
        const double lInterval = Maths::Max(InInterval, 0.0);
        if (lInterval == m_RefreshInterval && bInStated == m_bStatedRefresh)
        {
            return;
        }

        m_RefreshInterval   = lInterval;
        m_bStatedRefresh    = bInStated;
        m_Carried           = 0.0;
        m_MeasuredTime      = 0.0;
        m_MeasuredRefreshes = 0.0;
        m_OnRhythm          = 0;
        m_OffRhythm         = 0;
        m_bSnapping         = false;
    }

    double FrameClock::GetRefreshInterval() const noexcept
    {
        return (m_bStatedRefresh && m_MeasuredRefreshes >= MEASURED_REFRESHES) ? m_MeasuredTime / m_MeasuredRefreshes
                                                                                : m_RefreshInterval;
    }

    double FrameClock::SnapToRefresh(const double InMeasured) noexcept
    {
        if (m_RefreshInterval <= 0.0)
        {
            return InMeasured;
        }

        // The time measured and not given yet: a frame of whole refreshes leaves the rest to the next.
        const double lCarried   = m_Carried + InMeasured;
        const double lRefresh   = GetRefreshInterval();
        const double lRefreshes = Maths::RoundHalfFromZero(lCarried / lRefresh);
        const double lSnapped   = lRefreshes * lRefresh;

        // Off the screen's rhythm: a stall, or frames far shorter than a refresh (vsync not holding).
        const bool bOnRhythm = lRefreshes >= 1.0 && Maths::Abs(lCarried - lSnapped) <= SNAP_TOLERANCE * lRefresh;
        m_OnRhythm  = bOnRhythm ? m_OnRhythm + 1 : 0;
        m_OffRhythm = bOnRhythm ? 0 : m_OffRhythm + 1;

        if (m_OnRhythm >= RHYTHM_FRAMES)
        {
            m_bSnapping = true;
        }
        else if (m_OffRhythm >= RHYTHM_FRAMES)
        {
            m_bSnapping = false;
        }

        if (!bOnRhythm || !m_bSnapping)
        {
            // As measured; time given ahead of the timer is not taken back.
            m_Carried = 0.0;
            return Maths::Max(lCarried, 0.0);
        }

        m_Carried = lCarried - lSnapped;

        // A stated rate is whole hertz: the frames that keep to it measure the real one.
        if (m_bStatedRefresh)
        {
            m_MeasuredTime      += InMeasured;
            m_MeasuredRefreshes += lRefreshes;
        }
        return lSnapped;
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
