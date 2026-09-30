#include "Config_Engine.h"

namespace Opaax
{
    // =========================================================================
    // Config type tag (defined here so it is shared across the DLL/exe boundary).
    // =========================================================================
    ConfigTypeID Config_Engine::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ConfigTypeID>(&s_Tag);
    }
}
