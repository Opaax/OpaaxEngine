#include "Core/Time/DateSource.h"

namespace Opaax
{
    DateTime DateSource::Now() const
    {
        return m_bPinned ? m_Pinned : DateTime::Now();
    }

    void DateSource::Pin(const DateTime& InDate) noexcept
    {
        m_Pinned  = InDate;
        m_bPinned = true;
    }

    void DateSource::Unpin() noexcept
    {
        m_bPinned = false;
    }
}
