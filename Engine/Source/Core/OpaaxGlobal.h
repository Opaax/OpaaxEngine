#pragma once
#include "EngineAPI.h"
#include "OpaaxTypes.h"

namespace Opaax::OpaaxGlobal
{
    // =============================================================================
    // Global Values
    // =============================================================================

    /** Slot 0 of the intern pool: the value of an empty OpaaxStringID. */
    inline constexpr Uint32 ID_None = 0;

    /**
     * The text of ID_None. A literal (not an OpaaxString) so it is ready during static init.
     */
    inline constexpr const char* String_None = "None";
}
