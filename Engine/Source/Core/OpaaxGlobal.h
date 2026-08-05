#pragma once
#include "EngineAPI.h"
#include "String/OpaaxString.hpp"
#include "OpaaxTypes.h"

namespace Opaax::OpaaxGlobal
{
    // =============================================================================
    // Global Values
    // =============================================================================

    /** The intern pool's reserved slot 0 — what an empty or default OpaaxStringID holds. */
    inline constexpr Uint32 ID_None = 0;

    /** The text ID_None resolves to. Out-of-line: an OpaaxString is not a constant expression. */
    OPAAX_API extern const OpaaxString String_None;
}
