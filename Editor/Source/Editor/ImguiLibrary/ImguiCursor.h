#pragma once

#include "Core/Maths/MathTypes.h"

#include <imgui.h>

// =============================================================================
// ImguiCursor — moving the CURSOR ITSELF, which none of the other three files in this library do.
//   ImguiDraw paints, ImguiWidgets submits items, ImguiLayout is pure geometry; this reads and
//   writes ImGui's mouse state, so it is its own file rather than a guest in one of them.
// =============================================================================
namespace Opaax::Editor::ImguiCursor
{
    /**
     * Keep a live drag inside [InMin, InMax] by teleporting the cursor to the opposite edge when it
     * leaves — so a drag can run forever in one direction instead of dying at the panel border.
     *
     * Blender wraps at the region edge, Unreal captures and wraps the mouse, Unity calls
     * SetWantsMouseJumping during a handle drag. The mechanism here is ImGui's own
     * TeleportMousePos, which moves MousePos AND MousePosPrev — so MouseDelta reads ZERO on the
     * frame of the jump — and raises WantSetMousePos for the platform backend to reposition the OS
     * cursor on its next NewFrame.
     *
     * THAT ZEROED DELTA IS WHY THE TWO KINDS OF CONSUMER DIFFER, and the difference is the whole
     * reason this returns anything:
     *   - A consumer reading DELTAS (the camera pan) needs NOTHING else. It simply accumulates no
     *     motion for one frame, which is invisible.
     *   - A consumer reading the ABSOLUTE position (ImGuizmo) would see the cursor leap to the far
     *     side and fling the selection with it. It must accumulate the value returned here and add
     *     it back to io.MousePos, so the gizmo sees a cursor that walked off the screen and kept
     *     going.
     *
     * @param InMin Top-left of the rect to stay inside, in SCREEN pixels.
     * @param InMax Bottom-right, same space.
     * @param InMargin How far inside the opposite edge to land. Non-zero so a cursor that lands
     *   exactly on the border cannot wrap again next frame and oscillate.
     * @return The CORRECTION an absolute-position consumer must accumulate — the negation of the
     *   jump that was applied. {0,0} when nothing wrapped, which is almost every frame.
     */
    Vector2F WrapInRect(const ImVec2& InMin, const ImVec2& InMax, float InMargin = 4.f);
}
