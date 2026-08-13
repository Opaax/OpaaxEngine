#pragma once
#include "EngineAPI.h"
#include "OpaaxTypes.h"

namespace Opaax::OpaaxGlobal
{
    // =============================================================================
    // Global Values
    // =============================================================================

    /** The intern pool's reserved slot 0 — what an empty or default OpaaxStringID holds. */
    inline constexpr Uint32 ID_None = 0;

    /**
     * The text ID_None resolves to.
     *
     * A literal, NOT an `extern const OpaaxString`. The pool reads this from its own constructor, and
     * the pool is built lazily on the first OPAAX_ID(...) — which the tree already reaches during
     * static init (Renderer/RenderLayer.h's g_RenderLayerIDs). An out-of-line OpaaxString is
     * DYNAMICALLY initialised, TU init order inside the DLL is unspecified, and the ordering that
     * loses copies an uninitialised string. `constexpr const char*` is constant-initialised, so there
     * is no order to lose.
     */
    inline constexpr const char* String_None = "None";
}
