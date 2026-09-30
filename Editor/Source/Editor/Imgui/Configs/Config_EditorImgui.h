#pragma once

#include <Core/Config/TConfig.hpp>
#include "Editor/Imgui/Configs/EditorImguiConfigData.h"

namespace Opaax
{
    // Not exported: compiled into the editor exe, not the engine DLL (OPAAX_API would mean dllimport here).
    DECLARE_T_CONFIG(EditorImgui, EditorImguiConfigData)
}
