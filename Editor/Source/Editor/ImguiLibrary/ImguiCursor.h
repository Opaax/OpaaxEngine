#pragma once

#include "Core/Maths/MathTypes.h"

#include <imgui.h>

// =============================================================================
// ImguiCursor — moves the cursor itself (reads and writes ImGui's mouse state).
// =============================================================================
namespace Opaax::Editor::ImguiCursor
{
    /**
     * Keeps a live drag inside [InMin, InMax] by teleporting the cursor to the opposite edge when it
     * leaves, so a drag never stops at the panel border. Uses ImGui's TeleportMousePos, so MouseDelta
     * is zero on the frame of the jump.
     * - Delta consumers (camera pan) need nothing else.
     * - Absolute-position consumers (ImGuizmo) must add up the returned value and add it to
     *   io.MousePos, or the selection would jump to the far side.
     * @param InMin Top-left of the rect, in screen pixels
     * @param InMax Bottom-right, same space
     * @param InMargin How far inside the opposite edge to land (non-zero, so it cannot wrap again)
     * @return The correction to accumulate (the negated jump). {0,0} when nothing wrapped
     */
    Vector2F WrapInRect(const ImVec2& InMin, const ImVec2& InMax, float InMargin = 4.f);
}
