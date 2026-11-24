#pragma once

#include "Core/Time/DateTime.h"

namespace Opaax
{
    // =============================================================================
    // DateSource — where the date and time of day are read: the wall clock, or a date a test pinned
    //   (a menu showing today shows the same text every run). The engine keeps one (IEngine::GetDates);
    //   a UI canvas reads it (UICanvas::GetDates). A pinned date stands still: it does not tick.
    // =============================================================================
    class DateSource
    {
    public:
        /** The pinned date while one is, else DateTime::Now(). */
        DateTime Now() const;

        /** Now() answers InDate from now on, until Unpin. */
        void Pin(const DateTime& InDate) noexcept;

        /** Now() follows the wall clock again. */
        void Unpin() noexcept;

        bool IsPinned() const noexcept { return m_bPinned; }

    private:
        DateTime m_Pinned;
        bool     m_bPinned = false;
    };
}
