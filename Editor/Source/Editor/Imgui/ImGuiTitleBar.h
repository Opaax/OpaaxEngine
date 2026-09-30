#pragma once

#include "Editor/UI/IEditorGui.h"          // EWindowButtonKind, TitleBarDrag
#include "Editor/UI/EditorRectGeometry.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ImGuiTitleBar — the ImGui side of the caption: how the drag region and caption buttons look
    //   and report input, plus the window's resize border. What the bar contains and what input
    //   means is EditorTitleBar's. The resize border holds state (a drag spans frames).
    // =============================================================================
    class ImGuiTitleBar
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** IEditorGui::TitleBarDragRegion, in ImGui terms. Call inside the menu bar. */
        TitleBarDrag DragRegion(Uint32 InTrailingButtons);

        /** IEditorGui::TitleBarButton, in ImGui terms. @return True on the frame it is clicked. */
        bool Button(EWindowButtonKind InKind);

        /**
         * The 8-region resize border around the window, and its cursor. Submits no ImGui item (reads the
         * mouse, drives Window), so it runs once per frame after the pass.
         */
        void UpdateResizeBorder(EditorContext& InContext);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Not None while a border drag is live. */
        EWindowFrameEdge m_ResizeEdge = EWindowFrameEdge::None;
    };
}
