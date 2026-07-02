#include "IEngine.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullEngine — the locator's fallback when no engine is provided. Inert:
        // it owns nothing and drives nothing; IsNull() lets callers detect it.
        // =====================================================================
        class NullEngine final : public IEngine
        {
        public:
            bool IsNull() const noexcept override { return true; }
        };
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
    // =========================================================================
    ServiceTypeID IEngine::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IEngine& IEngine::Null()
    {
        static NullEngine s_Null;
        return s_Null;
    }
}
