#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // DateTime — a calendar date and time of day, in the local time zone: what a clock on the
    //   wall shows. For measures and timeouts, Clock (it never jumps).
    // =============================================================================
    struct DateTime
    {
        Int32 Year        = 1970;
        Int32 Month       = 1;    // 1 - 12
        Int32 Day         = 1;    // 1 - 31
        Int32 Hour        = 0;    // 0 - 23
        Int32 Minute      = 0;
        Int32 Second      = 0;
        Int32 Millisecond = 0;

        /** The local date and time now. */
        static DateTime Now();
    };
}
