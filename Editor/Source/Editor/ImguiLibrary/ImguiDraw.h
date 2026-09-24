#pragma once

#include "Editor/UI/IEditorUIBackend.h"   // EditorImage — what every image call here takes

#include <imgui.h>

// =============================================================================
// ImguiDraw — DRAW-LIST primitives. Everything here paints through an ImDrawList and SUBMITS NO
//   ITEM, which is the property callers depend on: the caller's own InvisibleButton/Selectable
//   stays the last-submitted item, so hover, click and drag-drop all keep working over the paint.
//
//   That is the axis this file is split on — see ImguiWidgets.h for the half that does submit.
// =============================================================================
namespace Opaax::Editor::ImguiDraw
{
    /** Scale a packed colour's RGB, alpha preserved. */
    ImU32 Darken(ImU32 InColor, float InFactor) noexcept;

    /** An EditorImage into [InMin, InMax], carrying its own UVs so no call site restates the flip. */
    void Image(ImDrawList* InDrawList, const EditorImage& InImage, ImVec2 InMin, ImVec2 InMax);

    /** The hover highlight every tile and row shares. */
    void HoverHighlight(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax);

    /** A folder: body + tab, in the browser's gold. */
    void FolderGlyph(ImDrawList* InDrawList, ImVec2 InMin, ImVec2 InMax);

    /**
     * A resource's icon in [InMin, InMax]: InImage when it is valid, else a card with InGlyph
     * centred in it. Both fill the SAME box, so a type gaining an icon does not move the layout.
     *
     * @param InInset Fraction of the box left as margin. The tile grid wants breathing room; a
     *   one-line row passes 0, where any inset would shrink the icon to nothing.
     */
    void IconBox(ImDrawList* InDrawList, const EditorImage& InImage, const char* InGlyph,
                 ImVec2 InMin, ImVec2 InMax, float InInset = 0.16f);
}
