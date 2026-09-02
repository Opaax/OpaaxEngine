#pragma once

#include <Core/Config/TConfig.hpp>
#include "Editor/Imgui/Configs/EditorImguiConfigData.h"

namespace Opaax
{
    // NOT exported: this config is compiled into the editor exe, not the engine DLL, so OPAAX_API
    // would resolve to dllimport on its own definition (C4273, then LNK2019 at the first call site).
    DECLARE_T_CONFIG(EditorImgui, EditorImguiConfigData)
}
