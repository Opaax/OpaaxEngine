#pragma once

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's NATIVE viewport tools — the strip over the viewport (③b), one function per tool.
    //
    //   Each matches ViewportToolbarRegistry::DrawFunc, so a plain function pointer registers.
    //   EditorService::RegisterNativeViewportTools states the ORDER and the grouping; what a tool
    //   DRAWS is here — the EditorNativeCommands shape, one route over.
    //
    //   A tool's whole input is the context it is handed (D3): the locator is not available to it,
    //   and a tool that INVOKES something dispatches a command by tag rather than reaching for the
    //   subject, so the toolbar, the Edit menu and W/E/R stay three front-ends onto one verb.
    //
    //   These are CONTENTS, not chrome, so they call ImGui directly — the deliberate exception
    //   IEditorGui carves out (MR2d).
    // =============================================================================
    namespace NativeViewportTools
    {
        /** Move / Rotate / Scale, as a radio group. Dispatches EDITOR_COMMAND_GIZMO_*. */
        void DrawGizmoMode(EditorContext& InContext);

        /** The snap toggle and the step for the ACTIVE mode. */
        void DrawSnap(EditorContext& InContext);

        /** The grid toggle. Its spacing is the translate snap step, so it belongs beside Snap. */
        void DrawGrid(EditorContext& InContext);

        /** Center / Origin / Individual — one button that cycles and names its state. */
        void DrawPivot(EditorContext& InContext);

        /** Local / World, disabled while the mode is Scale. */
        void DrawSpace(EditorContext& InContext);
    }
}
