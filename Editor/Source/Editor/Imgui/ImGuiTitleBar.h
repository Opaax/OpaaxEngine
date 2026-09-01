#pragma once

#include "Editor/UI/IEditorGui.h"          // EWindowButtonKind, TitleBarDrag
#include "Editor/UI/WindowFrameGeometry.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ImGuiTitleBar — the ImGui SIDE of the caption: how a drag region and a caption button look
    //   and report, plus the window's resize border.
    //
    //   WHAT the bar contains and what its input MEANS are not here — that is EditorTitleBar, which
    //   names no backend. This file is what a Qt port would replace and nothing else.
    //
    //   The resize border is deliberately NOT part of the bar: it is frame chrome around the whole
    //   window, and under a toolkit that keeps the OS frame it does not exist at all. It holds
    //   state because a drag spans frames.
    // =============================================================================
    class ImGuiTitleBar
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** IEditorGui::TitleBarDragRegion, in ImGui terms. Call inside the menu bar. */
        TitleBarDrag DragRegion(Uint32 InTrailingButtons);

        /** IEditorGui::TitleBarButton, in ImGui terms. @return true on the frame it is clicked. */
        bool Button(EWindowButtonKind InKind);

        /**
         * The 8-region resize border around the whole window, and the cursor that goes with it.
         *
         * Submits no ImGui item — it reads the mouse and drives Window directly — so it runs once
         * per frame after the pass, where IsAnyItemActive() already reflects what the panels took.
         */
        void UpdateResizeBorder(EditorContext& InContext);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Non-None while a border drag is live — it owns the frame until the button comes up. */
        EWindowFrameEdge m_ResizeEdge = EWindowFrameEdge::None;
    };
}
