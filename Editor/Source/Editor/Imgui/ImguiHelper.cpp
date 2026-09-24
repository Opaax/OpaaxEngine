#include "ImguiHelper.h"

#include "Core/Color/LinearColor.h"

ImVec2 Opaax::Editor::ImguiHelper::Vector2FToImVec2(const Vector2F& InVector2F)
{
    return ImVec2{InVector2F.x, InVector2F.y};
}

ImVec4 Opaax::Editor::ImguiHelper::LinearColorToImColor(const LinearColor& InColor)
{
    return ImVec4(InColor.x, InColor.y, InColor.z, InColor.w);
}
