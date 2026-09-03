#pragma once

#include "Engine/Subsystems/Input/InputCodes.h"   // EKeyCode — the editor's one key vocabulary
#include "Core/String/OpaaxString.hpp"
#include "Core/Maths/MathTypes.h"               // Vector2F — TitleBarDrag
#include "Editor/TitleBar/EditorTitleBar.h"     // owned by value — needs the complete type
#include "Editor/Panels/EditorPanels.h"         // likewise

namespace Opaax
{
    class Window;
}

namespace Opaax::Editor
{
    struct EditorContext;
    struct PanelWindowStyle;   // Editor/Panels/IEditorPanel.h — backend-agnostic already
    class PanelRegistry;
    class IEditorUIBackend;
    class IEditorWidgets;

    /** Which caption button — the seam names the KIND and lets each backend draw it. */
    enum class EWindowButtonKind : Uint8
    {
        Minimize,
        Maximize,
        Restore,
        Close
    };

    /** What the pointer did over the caption's draggable stretch this frame. */
    struct TitleBarDrag
    {
        /** Pointer movement while held, in screen pixels. Zero unless a drag is live. */
        Vector2F Delta{0.f, 0.f};

        bool bDoubleClicked = false;
    };

    // =============================================================================
    // IEditorGui — the editor's UI backend seam, and the owner of the whole UI pass.
    //
    //   Draw() emits everything: the dockspace, the menu bar and every panel window. Nothing
    //   outside this object decides the order they are drawn in.
    //
    //   It names no backend. ImGuiEditorGui is the one implementation today; a second one is a
    //   class swap in EditorService, not an edit spread across the editor.
    //
    //   A PANEL'S CONTENTS ARE NOT HERE, deliberately — a panel IS UI and calls the backend
    //   directly (MR2c). What this covers is the host CHROME around it.
    //
    //   The edges speak the ENGINE's vocabulary: a shortcut is an EKeyCode, and the two capture
    //   predicates are named for what they answer rather than for the backend's spelling.
    //
    //   No OPAAX_API: OpaaxEditorLib is a static lib archived into the editor exe, not a DLL.
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
         * Creates the UI context, applies the editor's config and style, points it at the dock
         * layout file, and brings up the backends for InWindow.
         *
         * @param InWindow The main window. Its GL context must be current on this thread.
         * @param InLayoutIniPath Absolute path to the dock layout, or EMPTY for "do not persist".
         *   STORED by the implementation rather than by the caller: ImGui BORROWS io.IniFilename
         *   and writes through it at DestroyContext, so the string has to outlive the context.
         * @return false when the window has no native handle — nothing is created, so nothing
         *   needs unwinding and the caller simply has no UI.
         */
        virtual bool Init(Window& InWindow, OpaaxString InLayoutIniPath) = 0;

        /** Backends down, then the context. The GL context must still be alive. Idempotent. */
        virtual void Shutdown() = 0;

        /**
         * Bring the whole UI stack down, IN ORDER: the panels, then the backend.
         *
         * NON-VIRTUAL, and that is the point. `ViewportPanel::Shutdown` frees an FBO and clears the
         * engine's primary render target, both of which need a live GL context (**F2a**) — so the
         * panels must die BEFORE the implementation destroys its context. That used to be two calls
         * sitting in the right order in `EditorService::OnShutdown`, i.e. a rule a future edit could
         * break silently. Owning both halves is what makes the order structural (**LC3**).
         */
        void Teardown()
        {
            m_Panels.Shutdown();
            Shutdown();
        }

        /** @return true once Init has succeeded — "the UI is up", the caller's pass-through gate. */
        virtual bool IsReady() const noexcept = 0;

        // End Lifecycle
        // =============================================================================

        // =============================================================================
        // Content — WHAT the pass draws: the two LIVE objects. Each is built from a registry the
        //   registrar owns, and each draws through the chrome below, so neither names a backend.
        //
        //   On the BASE rather than the implementation: a second backend would want the same menu
        //   tree and the same panels, not its own copies. What is backend-specific is the ~20
        //   virtual chrome calls below, not the content they draw.
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
         * Closes it: builds the draw data and submits it to the backbuffer. The host presents
         * afterwards. @pre IsReady()
         */
        virtual void EndFrame() = 0;

        /**
         * THE UI pass — dockspace, menu bar, panels, in that order.
         *
         * Non-const context because a menu click executes a command through it.
         */
        virtual void Draw(EditorContext& InContext) = 0;

        // End Frame
        // =============================================================================

        // =============================================================================
        // Chrome — the structural widgets the HOST emits, so EditorTitleBar and EditorPanels never
        //   name a backend. A panel's CONTENTS are not here: a panel IS UI and draws its own (MR2c).
    public:
        // NOTE: there is no BeginMenuBar here any more. OPENING the bar is the implementation's
        // own business — it is a row inside the editor's title bar, which the implementation
        // composes — so EditorTitleBar emits CATEGORIES and never the strip that holds them.

        /** A submenu. @return true when it is open, i.e. when children must be emitted. */
        virtual bool BeginMenu(const char* InLabel, bool bInEnabled) = 0;
        virtual void EndMenu() = 0;

        /**
         * One entry. No shortcut parameter: a menu node carries a command tag and nothing else
         * (MR2b), so there is no shortcut for it to display.
         *
         * @return true on the frame it is clicked.
         */
        virtual bool MenuItem(const char* InLabel, bool bInChecked, bool bInEnabled) = 0;

        /** A rule between entries. */
        virtual void MenuSeparator() = 0;

        /**
         * The draggable stretch of the caption — everything the menus and the trailing buttons
         * leave.
         *
         * Reports what the pointer DID; it does not act. Moving the window and toggling maximize are
         * EditorTitleBar's policy, so a second backend supplies input and inherits the behaviour.
         *
         * @param InTrailingButtons How many TitleBarButton calls follow, so the backend can reserve
         *   their width. A COUNT rather than a width: only the backend knows how wide it draws one.
         */
        virtual TitleBarDrag TitleBarDragRegion(Uint32 InTrailingButtons) = 0;

        /**
         * One caption button. @return true on the frame it is clicked.
         *
         * The KIND, not a label: every toolkit draws these its own way, and the glyph is the
         * backend's business (ImGui's default font has none of the characters they want).
         */
        virtual bool TitleBarButton(EWindowButtonKind InKind) = 0;

        /**
         * Open a panel's window: first-use size, the style's padding and the close button, in one
         * call — so a caller cannot get the push/pop pairing around it wrong.
         *
         * @param bOutWantOpen Set false when the user clicks the window's X. Give it a LOCAL and
         *   route the result through EditorPanels::SetVisible, the single mutation point (MR2c).
         * @return true when the body must be drawn. EndPanelWindow runs either way.
         */
        virtual bool BeginPanelWindow(const char* InLabel, const PanelWindowStyle& InStyle,
                                      bool& bOutWantOpen) = 0;

        /**
         * Whether the panel window BeginPanelWindow just opened has keyboard focus, its child
         * windows included.
         *
         * ASKED INSIDE THE BRACKET, which is what keeps it honest: "the window just begun" is
         * well-defined between BeginPanelWindow and EndPanelWindow and meaningless outside it. That
         * is a different thing from the decorate-the-previous-item query MR2h rules out of
         * IEditorWidgets — this seam is host chrome, and its windows are already an ordered pair.
         *
         * It exists so an authoring chord can mean the right thing: Ctrl+S saves the sheet when the
         * sheet editor has focus and the map otherwise.
         */
        virtual bool IsPanelWindowFocused() const = 0;

        /** Closes it. Runs whether or not BeginPanelWindow returned true. */
        virtual void EndPanelWindow() = 0;

        // End Chrome
        // =============================================================================

        // =============================================================================
        // Query
    public:
        /** The UI frame clock, in seconds — what a throttle measures against. */
        virtual double GetTime() const = 0;

        /**
         * The pointer is over SOME UI window.
         *
         * Named for what it answers, not for ImGui's `WantCaptureMouse`: in this editor one of
         * those windows is the game (the Viewport is an image with the world drawn into it), so
         * "the pointer is over the UI" and "the UI should own this click" are NOT the same
         * statement. A caller that means the second one still owes its own viewport carve-out
         * (SEL8, L29).
         */
        virtual bool IsPointerOverUI() const = 0;

        /**
         * A UI widget has the keyboard — true only for a text field. That IS the question a caller
         * wants, so no carve-out applies: a field with focus must win over any shortcut.
         */
        virtual bool IsKeyboardOwnedByUI() const = 0;

        /**
         * A globally-routed chord, in the engine's key vocabulary.
         *
         * Modifiers are just KEYS here, the same as InputManager states — pass EKeyCode::LeftControl
         * for Ctrl. Left and Right fold together.
         *
         * @return true on the frame the chord fires.
         */
        virtual bool Shortcut(EKeyCode InModifier, EKeyCode InKey) const = 0;

        /** The unmodified chord. Non-virtual: it is the one above with a fixed argument. */
        bool Shortcut(const EKeyCode InKey) const { return Shortcut(EKeyCode::None, InKey); }

        // End Query
        // =============================================================================

        // =============================================================================
        // Get - Set
    public:
        /**
         * The renderer-side integration, for the panels that turn a framebuffer or a texture into
         * an image. This is what EditorContext::UIBackend is built from.
         *
         * @pre IsReady()
         */
        virtual IEditorUIBackend& Backend() const noexcept = 0;

        /**
         * The VALUE-EDITOR vocabulary, for property drawers and anything else editing a field.
         *
         * Off the gui rather than owned separately because it is the SAME backend one level down,
         * exactly like Backend() - where IEditorDialogs is the OS and therefore is not.
         *
         * @pre IsReady()
         */
        virtual IEditorWidgets& Widgets() noexcept = 0;

        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    protected:
        // BOTH are LIVE objects built from a registry the registrar owns — the one shape, so the
        // next route that grows live state lands here without a decision.
        EditorTitleBar m_TitleBar;
        EditorPanels   m_Panels;
    };
}
