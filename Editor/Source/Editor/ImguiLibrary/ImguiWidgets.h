#pragma once

#include "Editor/UI/IEditorUIBackend.h"   // EditorImage

#include <imgui.h>

// =============================================================================
// ImguiWidgets — everything that SUBMITS AN ITEM: it advances the cursor and becomes the
//   last-submitted item, so IsItemHovered / drag-drop / the ID stack all apply to it.
//
//   The opposite half of ImguiDraw.h, and the split is the point: a caller that needs its own
//   button to stay the interactive item must reach for ImguiDraw, not for these.
// =============================================================================
namespace Opaax::Editor::ImguiWidgets
{
    /** An EditorImage at InSize, carrying its own UVs. A Dummy when invalid, so layout is unchanged. */
    void Image(const EditorImage& InImage, ImVec2 InSize);

    /** A SmallButton tinted while it is the active choice — a view/mode toggle. */
    bool ToggleButton(const char* InLabel, bool bActive);

    /**
     * One line of text ellipsized to InWidth, centred when InbCentered.
     *
     * A grid is unreadable if long names are allowed to set the column width, which is what this
     * exists to prevent — the truncation is measured, not a character count.
     */
    void TextEllipsized(const char* InText, float InWidth, bool bInCentered = false);
}
