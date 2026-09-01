#pragma once

#include "Editor/UI/WindowFrameGeometry.h"

namespace Opaax::Editor
{
    struct EditorContext;
    class EditorMenu;

    // =============================================================================
    // ImGuiTitleBar — the editor's own caption: the menu tree on the left, a drag region, and
    //   Minimize / Maximize / Close on the right. The OS draws none of it (Window::SetDecorated).
    //
    //   ImGuiEditorGui owns one and composes it into its host window; nothing outside the ImGui
    //   implementation names this type, which is what MR2d's chrome-vs-contents line asks for.
    //
    //   The MENU REGISTRY IS THE EXTENSION POINT — a game module's category appears in the bar
    //   through the route it already registers into, so there is no title-bar registry.
    //
    //   It holds state because a drag spans frames.
    // =============================================================================
    class ImGuiTitleBar
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * The bar's contents. Call between ImGui::BeginMenuBar and EndMenuBar.
         *
         * Order is menus, then the drag region taking the slack, then the buttons — so the
         * draggable area is whatever the other two do not claim, and a menu click cannot be
         * swallowed by it.
         */
        void DrawBar(EditorContext& InContext, const EditorMenu* InMenu);

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
