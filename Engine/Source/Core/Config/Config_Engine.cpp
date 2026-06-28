#include "Config_Engine.h"

namespace Opaax
{
    // =========================================================================
    // Config type tag (out-of-line — one tag across the DLL/exe line).
    // =========================================================================
    ConfigTypeID Config_Engine::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ConfigTypeID>(&s_Tag);
    }
}
