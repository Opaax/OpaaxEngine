#pragma once

#include "Editor/UI/IEditorUIBackend.h"           // owned through a TUniquePtr — needs the complete type
#include "Engine/Subsystems/Input/InputCodes.h"   // EKeyCode — the editor's one key vocabulary
#include "Core/OpaaxTypes.h"                      // TUniquePtr
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class Window;
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorGui — the editor's ImGui boundary: the context, the impl backends, the frame and the
    //   dockspace. EditorService owns one and delegates to it exactly as it delegates the menu bar
    //   to EditorMenu, so the composition root reads as editor policy and nothing outside a panel
    //   needs imgui.h to run a frame.
    //
    //   A PANEL is the deliberate exception — a panel IS UI and calls ImGui directly. What lives
    //   here is only what the SERVICE used to do.
    //
    //   It speaks the ENGINE's vocabulary at its edges: a shortcut is an EKeyCode (the same enum
    //   EditorService::HandleReservedKeys switches on), never an ImGuiKey, and the two capture
    //   predicates are named for what they answer rather than for ImGui's spelling of them.
    // =============================================================================
    class EditorGui
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorGui() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns the UI backend through a TUniquePtr (I6's corollary: an owner of a move-only member
        // must say so, or the implicit copy is instantiated anyway).
        EditorGui(const EditorGui&)            = delete;
        EditorGui& operator=(const EditorGui&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================

        // =============================================================================
        // Lifecycle
    public:
        /**
         * Creates the ImGui context, applies the editor's io config and style, points ImGui at the
         * dock layout file, and brings up the impl backends for InWindow.
         *
         * @param InWindow The main window. Its GL context must be current on this thread — the
         *   renderer impl's Init needs it.
         * @param InLayoutIniPath Absolute path to the dock layout, or EMPTY for "do not persist".
         *   STORED here rather than by the caller: ImGui BORROWS io.IniFilename and writes through
         *   it at DestroyContext, so the string has to outlive the context that reads it.
         * @return false when the window has no native handle — no context is created, so nothing
         *   needs unwinding and the caller simply has no UI.
         */
        bool Init(Window& InWindow, OpaaxString InLayoutIniPath);

        /**
         * Impl backends down, then the context — the order ImGui requires, and the GL context must
         * still be alive for both. Idempotent.
         */
        void Shutdown();

        /** @return true once Init has succeeded — "the UI is up", the caller's pass-through gate. */
        bool IsReady() const noexcept { return m_Backend != nullptr; }

        // End Lifecycle
        // =============================================================================

        // =============================================================================
        // Frame
    public:
        /**
         * Opens the ImGui frame: impl backends, then ImGui, then ImGuizmo.
         * @pre IsReady()
         */
        void BeginFrame();

        /**
         * Closes it: builds the draw data, submits it to the backbuffer, and updates the platform
         * windows when multi-viewport is on. The host presents afterwards.
         * @pre IsReady()
         */
        void EndFrame();

        /** The dockspace every panel docks into, over the main viewport. */
        void DrawDockspace();

        // End Frame
        // =============================================================================

        // =============================================================================
        // Query
    public:
        /** ImGui's frame clock, in seconds — what a throttle measures against. */
        double GetTime() const;

        /**
         * The pointer is over SOME ImGui window.
         *
         * Named for what it answers, not for ImGui's `WantCaptureMouse`: in this editor one of those
         * windows is the game (the Viewport is an image with the world drawn into it), so "the
         * pointer is over the UI" and "the UI should own this click" are NOT the same statement.
         * A caller that means the second one still owes its own viewport carve-out (SEL8, L29).
         */
        bool IsPointerOverUI() const;

        /**
         * A UI widget has the keyboard — ImGui's `WantCaptureKeyboard`, which only goes true for a
         * text field. That IS the question a caller wants, so no carve-out applies: a field with
         * focus must win over any shortcut.
         */
        bool IsKeyboardOwnedByUI() const;

        /**
         * A globally-routed chord, in the engine's key vocabulary.
         *
         * Modifiers are just KEYS here, the same as InputManager states — pass EKeyCode::LeftControl
         * for Ctrl. ImGui's chord modifiers are side-agnostic, so Left and Right fold together.
         *
         * @return true on the frame the chord fires.
         */
        bool Shortcut(EKeyCode InModifier, EKeyCode InKey) const;

        /** The unmodified chord. */
        bool Shortcut(const EKeyCode InKey) const { return Shortcut(EKeyCode::None, InKey); }

        // End Query
        // =============================================================================

        // =============================================================================
        // Get - Set
    public:
        /**
         * The renderer-side integration, for the panels that turn a framebuffer or a texture into
         * an ImGui image. This is what EditorContext::UIBackend is built from.
         *
         * @pre IsReady()
         */
        IEditorUIBackend& Backend() const noexcept { return *m_Backend; }

        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IEditorUIBackend> m_Backend;

        // ImGui stores io.IniFilename as a BORROWED const char* — it never copies the string — so
        // this must stay alive, and unmodified, until DestroyContext() (which saves through that
        // very pointer). Assigned once in Init(); never cleared in Shutdown().
        OpaaxString                  m_LayoutIniPath;
    };
}
