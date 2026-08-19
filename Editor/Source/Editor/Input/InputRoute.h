#pragma once

#include "Core/OpaaxTypes.h"   // Uint8

namespace Opaax
{
    class InputManager;
    class WorldManager;
}

namespace Opaax::Editor
{
    class PlayInEditor;

    // =============================================================================
    // EInputRouteState — whether the engine is being fed, and if not, WHY.
    //
    //   The "why" is not decoration: every closed state is a different user-visible situation
    //   ("nothing happens when I press W"), and naming them is what makes the Input panel able to
    //   answer that question instead of just showing a dead key list.
    // =============================================================================
    enum class EInputRouteState : Uint8
    {
        Open,               // the engine is fed
        ClosedNoWorld,      // nothing to play
        ClosedViewport,     // the viewport is neither hovered nor focused (D5 step 2)
        ClosedEditMode,     // the active world is Edit — editor tools own it (D5 step 4)
        ClosedPaused        // a PIE session is paused; a frozen game must not accumulate input
    };

    const char* ToString(EInputRouteState InState) noexcept;

    // =============================================================================
    // InputRoute — the editor's answer to "is the engine being fed right now?" (Editor.md D5).
    //
    //   ONE definition of the rule, two consumers: EditorService::RouteInput gates on it, and the
    //   Input panel displays it. Duplicating the condition in the panel would let the display and
    //   the behaviour drift, which is the failure mode that makes an instrument worse than none.
    //
    //   The rule is a conjunction, evaluated in D5's own order so the REASON reported is the first
    //   thing that is wrong: a world exists, the viewport has the pointer or the keyboard, the
    //   world is in Play, and a PIE session is not paused.
    //
    //   CLOSING RESETS THE ENGINE'S INPUT. That is the whole reason this is a stateful object
    //   rather than a free function: the transition open -> closed is the event, and on it the
    //   engine must forget everything held, or a key released while the editor had focus stays
    //   held forever (D5's stuck-key contract).
    // =============================================================================
    class InputRoute
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        InputRoute(WorldManager& InWorlds, InputManager& InInput, const PlayInEditor& InPIE) noexcept
            : m_Worlds(InWorlds), m_Input(InInput), m_PIE(InPIE) {}

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        InputRoute(const InputRoute&)            = delete;
        InputRoute& operator=(const InputRoute&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Re-decide whether the route is open, and reset the engine's input if it just closed.
         *
         * Called ONCE PER FRAME by EditorService::BeginFrame — not from RouteInput, which only
         * runs when an event happens to arrive. A rule evaluated only on input cannot notice that
         * input stopped, which is exactly the case that has to trigger the reset.
         */
        void Evaluate();

        /**
         * The viewport reports where the pointer and the keyboard are. ImGui can only answer that
         * while the panel's window is current, so the panel is the only place that can measure it —
         * and the values are read one frame later, the same lag its resize handshake already lives
         * with.
         *
         * PUSHED rather than read back out of the panel: this object is the one answer to "is the
         * engine being fed", so the fact belongs beside the rule that consumes it, and nothing else
         * needs a typed pointer to a panel. Cleared every frame in ViewportPanel::OnPreRender, so a
         * HIDDEN viewport reports false instead of holding the last value it measured.
         */
        void SetViewportFocus(bool bInHovered, bool bInFocused) noexcept;

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** @return true when the application should feed the engine (D5's steps 2 + 4 passed). */
        bool IsOpen() const noexcept { return m_State == EInputRouteState::Open; }

        EInputRouteState GetState() const noexcept { return m_State; }

        /** D5 step 1's exemption: ImGui owns the pointer over the UI, the game owns it over the viewport. */
        bool IsViewportHovered() const noexcept { return m_bViewportHovered; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager&       m_Worlds;
        InputManager&       m_Input;
        const PlayInEditor& m_PIE;

        // Starts CLOSED: the editor opens on an Edit world, so "open" would be wrong for the one
        // frame before the first Evaluate — and would log a close that never happened.
        EInputRouteState m_State = EInputRouteState::ClosedNoWorld;

        // Pushed by ViewportPanel. False until it has drawn once — before that there is nothing
        // for the pointer to be over.
        bool m_bViewportHovered = false;
        bool m_bViewportFocused = false;
    };
}
