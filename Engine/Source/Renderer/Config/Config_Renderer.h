#pragma once

#include <Core/Config/TConfig.hpp>
#include <Renderer/Config/RendererConfigData.h>

#include "Core/EngineAPI.h"

namespace Opaax
{
    // EXPORTED, and that is not decoration: IMPL_T_CONFIG defines StaticTypeID() in this module's
    // .cpp, so the editor naming this type to register a drawer for it is an LNK2019 without
    // OPAAX_API — I6's tell in mirror image, exported-ness only ever exercised from inside the DLL.
    DECLARE_OPAAX_T_CONFIG(Renderer, RendererConfigData)
}
