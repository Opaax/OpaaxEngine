#include "FlyMoveParams.h"

#if OPAAX_WITH_EDITOR
#include <imgui.h>
#endif

namespace Opaax
{
    void FlyMoveParams::DrawEditor()
    {
#if OPAAX_WITH_EDITOR
        ImGui::DragFloat("Max Speed", &MaxSpeed, 1.f, 0.f, 0.f, "%.0f");
#endif
    }

} // namespace Opaax
