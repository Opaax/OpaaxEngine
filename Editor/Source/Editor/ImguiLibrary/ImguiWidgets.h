#pragma once

#include "Editor/UI/IEditorUIBackend.h"   // EditorImage

#include <imgui.h>

// =============================================================================
// ImguiWidgets — helpers that submit an ImGui item (IsItemHovered, drag-drop and the ID stack
//   apply to them). To keep your own button as the interactive item, use ImguiDraw instead.
// =============================================================================
namespace Opaax::Editor::ImguiWidgets
{
    /** An EditorImage at InSize, using its own UVs. A Dummy when invalid, so the layout is unchanged. */
    void Image(const EditorImage& InImage, ImVec2 InSize);

    /** A SmallButton tinted while it is the active choice (a view or mode toggle). */
    bool ToggleButton(const char* InLabel, bool bActive);

    /**
     * One line of text shortened with an ellipsis to fit InWidth, centred when InbCentered.
     */
    void TextEllipsized(const char* InText, float InWidth, bool bInCentered = false);
}
