#pragma once
#include <imgui.h>

#include "Core/Maths/MathTypes.h"

namespace Opaax {
    struct LinearColor;
}

namespace Opaax::Editor{

struct ImguiHelper
{

    static ImVec2 Vector2FToImVec2(const Vector2F& InVector2F);
    static ImVec4 LinearColorToImColor(const LinearColor& InColor);
    
};
}
