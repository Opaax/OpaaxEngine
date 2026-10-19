#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// Clock — the steady clock: it never goes back and is not the time of day (DateTime is), so only
//   the difference of two readings means something. For measures, timeouts and profiling.
// =============================================================================
namespace Opaax::Clock
{
    /** Seconds since an unspecified start (the machine's boot, usually). */
    double NowSeconds() noexcept;

    /** Nanoseconds since the same start: for short measures a double would round. */
    Uint64 NowNanoseconds() noexcept;
}
