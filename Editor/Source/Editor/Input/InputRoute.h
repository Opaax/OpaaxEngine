#pragma once

#include "Core/OpaaxTypes.h"        // Uint8
#include "Core/Maths/MathTypes.h"   // Vector2F

namespace Opaax
{
    class InputManager;
    class WorldManager;
}

namespace Opaax::Editor
{
    class PlayInEditor;

    // =============================================================================
    // EInputRouteState — whether the engine is fed, and if not, why (shown by the Input panel).
    // =============================================================================
    enum class EInputRouteState : Uint8
    {
        Open,               // the engine is fed
        ClosedNoWorld,      // nothing to play
        ClosedViewport,     // the viewport is neither hovered nor focused
        ClosedEditMode,     // the active world is Edit (editor tools own it)
        ClosedPaused        // the game is paused; it must not store input
    };

    const char* ToString(EInputRouteState InState) noexcept;

    // =============================================================================
    // InputRoute — whether the engine is fed input right now. EditorService::RouteInput gates on it,
    //   the Input panel displays it. Open when: a world exists, the viewport has the pointer or the
    //   keyboard, the world is in Play, and the game is not paused. The first failed check is the
    //   reason reported.
    //   Closing resets the engine's input, otherwise a key released while the editor had focus stays
    //   held.
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
         * Decides whether the route is open, and resets the engine's input if it just closed.
         * Called once per frame by EditorService::BeginFrame (not only when input arrives, or a stop in
         * input would go unnoticed).
         */
        void Evaluate();

        /**
         * The viewport reports whether it has the pointer and the keyboard. Only the panel can measure
         * it (inside its ImGui window); read one frame later. Cleared every frame in
         * ViewportPanel::OnPreRender, so a hidden viewport reports false.
         */
        void SetViewportFocus(bool bInHovered, bool bInFocused) noexcept;

        /**
         * The pointer position inside the viewport image, in pixels. Pushed by ViewportPanel, fed to the
         * engine's InputManager in Evaluate while the route is open.
         */
        void SetPointerLocalPx(const Vector2F& InLocalPx) noexcept { m_PointerLocalPx = InLocalPx; }

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** @return True when the application should feed the engine. */
        bool IsOpen() const noexcept { return m_State == EInputRouteState::Open; }

        EInputRouteState GetState() const noexcept { return m_State; }

        /** Whether the game owns the pointer (over the viewport) rather than ImGui (over the UI). */
        bool IsViewportHovered() const noexcept { return m_bViewportHovered; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager&       m_Worlds;
        InputManager&       m_Input;
        const PlayInEditor& m_PIE;

        // Starts closed: the editor opens on an Edit world.
        EInputRouteState m_State = EInputRouteState::ClosedNoWorld;

        // Pushed by ViewportPanel. False until it has drawn once.
        bool m_bViewportHovered = false;
        bool m_bViewportFocused = false;

        // The pointer inside the viewport image, pushed by ViewportPanel. Fed to the engine while Open.
        Vector2F m_PointerLocalPx = { 0.f, 0.f };
    };
}
