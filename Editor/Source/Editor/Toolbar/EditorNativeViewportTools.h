#pragma once

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's native viewport tools: the strip over the viewport, one function per tool.
    //   Each matches ViewportToolbarRegistry::DrawFunc. EditorService::RegisterNativeViewportTools sets
    //   the order. A tool only uses the context it is given, and dispatches commands by tag.
    //   Tools are contents, so they call ImGui directly.
    // =============================================================================
    namespace NativeViewportTools
    {
        /** Move / Rotate / Scale, as a radio group. Dispatches EDITOR_COMMAND_GIZMO_*. */
        void DrawGizmoMode(EditorContext& InContext);

        /** The snap toggle and the step of the active mode. */
        void DrawSnap(EditorContext& InContext);

        /** The grid toggle. Its spacing is the translate snap step. */
        void DrawGrid(EditorContext& InContext);

        /**
         * The collider-outline toggle. Flips the engine's DebugDraw channel (also used in a dev Game.exe).
         */
        void DrawColliders(EditorContext& InContext);

        /** Center / Origin / Individual: one button that cycles and shows its state. */
        void DrawPivot(EditorContext& InContext);

        /** Local / World, disabled while the mode is Scale. */
        void DrawSpace(EditorContext& InContext);
    }
}
