#include "Editor/ImguiLibrary/ImguiCursor.h"

// TeleportMousePos is internal-header API. It is the ONLY thing that sets MousePos, MousePosPrev
// and WantSetMousePos together — writing io.MousePos by hand would leave MousePosPrev at the old
// value, so ImGui would report the jump as a MouseDelta and every drag in the frame would lurch.
#include <imgui_internal.h>

namespace Opaax::Editor
{
    Vector2F ImguiCursor::WrapInRect(const ImVec2& InMin, const ImVec2& InMax, const float InMargin)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        // A rect too small to hold the margins on both sides would teleport the cursor straight back
        // out, so it is left alone entirely.
        const float lWidth  = InMax.x - InMin.x;
        const float lHeight = InMax.y - InMin.y;

        if (lWidth <= InMargin * 2.f || lHeight <= InMargin * 2.f)
        {
            return { 0.f, 0.f };
        }

        if (!ImGui::IsMousePosValid(&lIO.MousePos))
        {
            return { 0.f, 0.f };   // the cursor left the window entirely
        }

        ImVec2 lTarget = lIO.MousePos;

        // Each axis independently: dragging into a corner should wrap both, and an axis that never
        // reaches its edge must not be nudged.
        if (lIO.MousePos.x < InMin.x)      { lTarget.x = InMax.x - InMargin; }
        else if (lIO.MousePos.x > InMax.x) { lTarget.x = InMin.x + InMargin; }

        if (lIO.MousePos.y < InMin.y)      { lTarget.y = InMax.y - InMargin; }
        else if (lIO.MousePos.y > InMax.y) { lTarget.y = InMin.y + InMargin; }

        const Vector2F lJump{ lTarget.x - lIO.MousePos.x, lTarget.y - lIO.MousePos.y };

        if (lJump.x == 0.f && lJump.y == 0.f)
        {
            return { 0.f, 0.f };
        }

        ImGui::TeleportMousePos(lTarget);

        // The NEGATION: an absolute-position consumer adds this back, so its view of the cursor
        // continues in the direction the drag was going instead of snapping to the far edge.
        return { -lJump.x, -lJump.y };
    }
}
