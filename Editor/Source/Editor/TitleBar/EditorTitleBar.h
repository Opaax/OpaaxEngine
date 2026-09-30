#pragma once

namespace Opaax
{
    class Window;
}

namespace Opaax::Editor
{
    struct EditorContext;
    class IEditorGui;
    class TitleBarRegistry;

    // =============================================================================
    // EditorTitleBar — the editor's caption bar: registered menus on the left, a drag region, and
    //   Minimize / Maximize / Close on the right. Build() takes the registry, Draw() walks it.
    //   Names no UI backend: menus go through IEditorGui, window moves through EditorContext. The
    //   behaviour lives here (drag moves, double-click toggles maximize); the backend only reports
    //   what the pointer did.
    // =============================================================================
    class EditorTitleBar
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Takes the sealed registry (borrowed, it outlives the gui). */
        void Build(const TitleBarRegistry& InRegistry) noexcept { m_Registry = &InRegistry; }

        /**
         * Draws the bar's contents. The caller has already opened the strip.
         */
        void Draw(EditorContext& InContext, IEditorGui& InGui) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Null until Build; Draw then shows only the buttons. */
        const TitleBarRegistry* m_Registry = nullptr;
    };
}
