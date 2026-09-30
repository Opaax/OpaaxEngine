#include "Editor/ImguiLibrary/ImguiCursor.h"

// TeleportMousePos is internal API. It sets MousePos, MousePosPrev and WantSetMousePos together;
// writing io.MousePos alone would show the jump as a MouseDelta.
#include <imgui_internal.h>

namespace Opaax::Editor
{
    Vector2F ImguiCursor::WrapInRect(const ImVec2& InMin, const ImVec2& InMax, const float InMargin)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        // A rect too small for the margins on both sides is left alone (the cursor would bounce back).
        const float lWidth  = InMax.x - InMin.x;
        const float lHeight = InMax.y - InMin.y;

        if (lWidth <= InMargin * 2.f || lHeight <= InMargin * 2.f)
        {
            return { 0.f, 0.f };
        }

        if (!ImGui::IsMousePosValid(&lIO.MousePos))
        {
            return { 0.f, 0.f };   // the cursor left the window
        }

        ImVec2 lTarget = lIO.MousePos;

        // Each axis on its own: a corner wraps both, an axis that stays inside is not touched.
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

        // The negation: an absolute-position consumer adds it back, so the cursor seems to keep going.
        return { -lJump.x, -lJump.y };
    }
}
