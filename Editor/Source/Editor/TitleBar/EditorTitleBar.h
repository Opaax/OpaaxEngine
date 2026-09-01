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
    // EditorTitleBar — the editor's caption, LIVE: the registered menu categories on the left, a
    //   drag region, and Minimize / Maximize / Close on the right.
    //
    //   The EditorPanels shape, one route over: Build() takes the registry, Draw() walks it. That
    //   symmetry is the point — TitleBarRegistry is what modules REGISTER, this is what it BECOMES,
    //   and only the backend below differs between UI toolkits.
    //
    //   IT NAMES NO BACKEND. Menus go through IEditorGui::BeginMenu/MenuItem, the caption furniture
    //   through IEditorGui's title-bar chrome, and a window move through EditorContext::MainWindow.
    //   The POLICY lives here — a drag moves the window, a double-click toggles maximize, the
    //   middle glyph follows the state — while the backend only reports what the pointer did.
    //
    //   Why it is not merely a wrapper around the registry: the bar's own FURNITURE is not
    //   registered by anyone. That is the exact counterpart of EditorPanels owning per-panel
    //   visibility on top of PanelRegistry's descriptions.
    // =============================================================================
    class EditorTitleBar
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Adopt the sealed registry. Borrowed, not copied — the registrar outlives the gui. */
        void Build(const TitleBarRegistry& InRegistry) noexcept { m_Registry = &InRegistry; }

        /**
         * Emit the bar's contents.
         *
         * The caller has already opened the strip — which is what lets the buttons share the row
         * with the menus, and what keeps "where the bar sits" the backend's business.
         */
        void Draw(EditorContext& InContext, IEditorGui& InGui) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Null until Build; Draw then emits the furniture and no menus. */
        const TitleBarRegistry* m_Registry = nullptr;
    };
}
