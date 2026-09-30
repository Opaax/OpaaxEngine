#pragma once

#include "Engine/Subsystems/Input/InputCodes.h"   // EKeyCode
#include "Core/String/OpaaxString.hpp"
#include "Core/Maths/MathTypes.h"               // Vector2F
#include "Editor/TitleBar/EditorTitleBar.h"     // owned by value (needs the complete type)
#include "Editor/Panels/EditorPanels.h"         // same

namespace Opaax
{
    class Window;
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct PanelWindowStyle;   // Editor/Panels/IEditorPanel.h
    class PanelRegistry;
    class IEditorUIBackend;
    class IEditorWidgets;

    /** Which caption button. The backend decides how to draw it. */
    enum class EWindowButtonKind : Uint8
    {
        Minimize,
        Maximize,
        Restore,
        Close
    };

    /**
     * The font the editor's own UI uses: a file, a size, and fallback faces for other scripts (so
     * text in other alphabets typed in the Inspector does not show as boxes). Each backend applies
     * it its own way.
     */
    struct EditorUIFont
    {
        /** Absolute path to the primary face. Empty keeps the backend's default. */
        OpaaxString Path;

        /**
         * Extra faces merged into the same font, for scripts the primary does not cover (font files are
         * often split per script).
         */
        TDynArray<OpaaxString> Fallbacks;

        float SizePx = 16.f;
    };

    /** What the pointer did over the caption's drag region this frame. */
    struct TitleBarDrag
    {
        /** Pointer movement while held, in screen pixels. Zero unless dragging. */
        Vector2F Delta{0.f, 0.f};

        bool bDoubleClicked = false;
    };

    // =============================================================================
    // IEditorGui — the editor's UI backend interface, and owner of the whole UI pass.
    //   Draw() draws everything: the dockspace, the title bar and every panel window, in a fixed order.
    //   ImGuiEditorGui is the one implementation. Panel contents are not here (panels call the
    //   backend directly); this covers the window chrome around them.
    //   Shortcuts use EKeyCode.
    //   No OPAAX_API: OpaaxEditorLib is a static lib linked into the editor exe.
    // =============================================================================
    class IEditorGui
    {
        // =============================================================================
        // Dtor
        // =============================================================================
    public:
        virtual ~IEditorGui() = default;

        // =============================================================================
        // Functions
        // =============================================================================

        // =============================================================================
        // Lifecycle
    public:
        /**
         * Creates the UI context, applies the editor's config and style, sets the dock layout file, and
         * starts the backends for InWindow.
         * @param InWindow The main window. Its GL context must be current on this thread
         * @param InLayoutIniPath Absolute path to the dock layout, or empty to not save it. Stored by the
         *   implementation (ImGui keeps a pointer to it until the context is destroyed)
         * @return False when the window has no native handle (nothing is created)
         */
        virtual bool Init(Window& InWindow, OpaaxString InLayoutIniPath) = 0;

        /**
         * Draws the UI in InFont from now on. A face that cannot be read is logged and skipped (the
         * editor stays usable in its default font).
         * @param InFont Absolute paths. An empty primary keeps the backend's default
         */
        virtual void SetUIFont(const EditorUIFont& InFont) = 0;

        /** Shuts the backends down, then the context. The GL context must still be alive. Idempotent. */
        virtual void Shutdown() = 0;

        /**
         * Shuts the whole UI down, in order: panels first, then the backend. Non-virtual on purpose:
         * panels such as the Viewport free GL resources and need the context alive.
         */
        void Teardown()
        {
            m_Panels.Shutdown();
            Shutdown();
        }

        /** @return True once Init has succeeded. */
        virtual bool IsReady() const noexcept = 0;

        // End Lifecycle
        // =============================================================================

        // =============================================================================
        // Content — what the pass draws: the title bar and the panels. Both are built from registries
        //   and draw through the chrome below, so neither names a backend.
    public:
        EditorTitleBar&       TitleBar()       noexcept { return m_TitleBar; }
        const EditorTitleBar& TitleBar() const noexcept { return m_TitleBar; }

        EditorPanels&       Panels()       noexcept { return m_Panels; }
        const EditorPanels& Panels() const noexcept { return m_Panels; }

        // End Content
        // =============================================================================

        // =============================================================================
        // Frame
    public:
        /** Opens the UI frame. @pre IsReady() */
        virtual void BeginFrame() = 0;

        /**
         * Closes the UI frame: builds the draw data and submits it. The host presents afterwards.
         * @pre IsReady()
         */
        virtual void EndFrame() = 0;

        /**
         * The UI pass: dockspace, title bar, panels, in that order. Non-const context: a menu click runs
         * a command.
         */
        virtual void Draw(EditorContext& InContext) = 0;

        // End Frame
        // =============================================================================

        // =============================================================================
        // Chrome — the structural widgets the host draws, so EditorTitleBar and EditorPanels never name
        //   a backend.
    public:
        // No BeginMenuBar: the menu row is part of the title bar the implementation draws, so
        // EditorTitleBar only emits categories.

        /** A submenu. @return True when it is open (emit its children). */
        virtual bool BeginMenu(const char* InLabel, bool bInEnabled) = 0;
        virtual void EndMenu() = 0;

        /**
         * One menu entry (no shortcut: a menu node only carries a command tag).
         * @return True on the frame it is clicked
         */
        virtual bool MenuItem(const char* InLabel, bool bInChecked, bool bInEnabled) = 0;

        /** A rule between entries. */
        virtual void MenuSeparator() = 0;

        /**
         * The caption's drag region: whatever the menus and the trailing buttons leave. Only reports
         * what the pointer did; EditorTitleBar decides what to do with it.
         * @param InTrailingButtons How many TitleBarButton calls follow, so their width is reserved
         */
        virtual TitleBarDrag TitleBarDragRegion(Uint32 InTrailingButtons) = 0;

        /**
         * One caption button, by kind (the backend draws the glyph).
         * @return True on the frame it is clicked
         */
        virtual bool TitleBarButton(EWindowButtonKind InKind) = 0;

        /**
         * Opens a panel's window: first-use size, style padding and close button, in one call.
         * @param bOutWantOpen Set false when the user clicks the X. Pass a local and apply it through
         *   EditorPanels::SetVisible
         * @return True when the body must be drawn. Call EndPanelWindow either way
         */
        virtual bool BeginPanelWindow(const char* InLabel, const PanelWindowStyle& InStyle,
                                      bool& bOutWantOpen) = 0;

        /**
         * Whether the window just opened by BeginPanelWindow (or one of its children) has keyboard focus.
         * Only valid between BeginPanelWindow and EndPanelWindow. Lets Ctrl+S save the focused document.
         */
        virtual bool IsPanelWindowFocused() const = 0;

        /** Closes the panel window. Call it whether or not BeginPanelWindow returned true. */
        virtual void EndPanelWindow() = 0;

        // End Chrome
        // =============================================================================

        // =============================================================================
        // Query
    public:
        /** The UI frame clock, in seconds. */
        virtual double GetTime() const = 0;

        /**
         * The pointer is over some UI window. Note: the Viewport is a UI window too, so this does not mean
         * the UI should take the click; callers handle the viewport themselves.
         */
        virtual bool IsPointerOverUI() const = 0;

        /**
         * A text field has the keyboard. A focused field wins over any shortcut.
         */
        virtual bool IsKeyboardOwnedByUI() const = 0;

        /**
         * A global shortcut chord. Modifiers are keys (EKeyCode::LeftControl for Ctrl); left and right
         * count the same.
         * @return True on the frame the chord fires
         */
        virtual bool Shortcut(EKeyCode InModifier, EKeyCode InKey) const = 0;

        /** The chord without modifiers. */
        bool Shortcut(const EKeyCode InKey) const { return Shortcut(EKeyCode::None, InKey); }

        // End Query
        // =============================================================================

        // =============================================================================
        // Get - Set
    public:
        /**
         * The renderer-side integration, for panels that show a framebuffer or a texture as an image.
         * EditorContext::UIBackend is built from it.
         * @pre IsReady()
         */
        virtual IEditorUIBackend& Backend() const noexcept = 0;

        /**
         * The value-editor widgets, for property drawers and other field editors.
         * @pre IsReady()
         */
        virtual IEditorWidgets& Widgets() noexcept = 0;

        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        // Both are built from registries the registrar owns.
        EditorTitleBar m_TitleBar;
        EditorPanels   m_Panels;
    };
}
