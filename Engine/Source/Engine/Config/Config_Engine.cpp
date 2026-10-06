#include "Engine/Config/Config_Engine.h"

namespace Opaax
{
    // =========================================================================
    // Config type tag (defined here so there is exactly one).
    // =========================================================================
    ConfigTypeID Config_Engine::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ConfigTypeID>(&s_Tag);
    }
}
