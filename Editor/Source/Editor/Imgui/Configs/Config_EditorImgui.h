#pragma once

#include "Engine/Config/TConfig.h"
#include "Editor/Imgui/Configs/EditorImguiConfigData.h"

namespace Opaax
{
    // The editor's look. Not registered with OPAAX_REGISTER_OBJECT (the editor library is not linked
    // whole, so the registration would never run): the editor makes it when it reads it.
    DECLARE_T_CONFIG(EditorImgui, EditorImguiConfigData)
}
