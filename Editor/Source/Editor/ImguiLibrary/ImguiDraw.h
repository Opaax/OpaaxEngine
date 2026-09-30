#pragma once

#include "Editor/UI/IEditorUIBackend.h"   // EditorImage

#include <imgui.h>

// =============================================================================
// ImguiDraw — draw-list primitives. Nothing here submits an ImGui item, so the caller's own
//   button stays the last item (hover, click and drag-drop keep working). See ImguiWidgets.h for
//   helpers that do submit items.
// =============================================================================
namespace Opaax::Editor::ImguiDraw
{
    /** Scales a packed colour's RGB, keeping alpha. */
    ImU32 Darken(ImU32 InColor, float InFactor) noexcept;

    /** Draws an EditorImage into [InMin, InMax], using its own UVs. */
    void Image(ImDrawList* InDrawList, const EditorImage& InImage, ImVec2 InMin, ImVec2 InMax);

    /** The hover highlight shared by tiles and rows. */
    void HoverHighlight(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax);

    /** A folder: body + tab, in the browser's gold. */
    void FolderGlyph(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax);

    /**
     * A resource's icon in [InMin, InMax]: InImage when valid, else a card with InGlyph centred. Both
     * fill the same box, so adding an icon does not move the layout.
     * @param InInset Fraction of the box left as margin (0 for a one-line row)
     */
    void IconBox(ImDrawList* InDrawList, const EditorImage& InImage, const char* InGlyph,
                 ImVec2 InMin, ImVec2 InMax, float InInset = 0.16f);
}
