#include "Editor/Input/InputRoute.h"

#include "Editor/PIE/PlayInEditor.h"

#include "Engine/Subsystems/Input/InputManager.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace Opaax::Editor
{
    const char* ToString(EInputRouteState InState) noexcept
    {
        switch (InState)
        {
        case EInputRouteState::Open:           return "OPEN";
        case EInputRouteState::ClosedNoWorld:  return "CLOSED — no active world";
        case EInputRouteState::ClosedViewport: return "CLOSED — viewport not hovered or focused";
        case EInputRouteState::ClosedEditMode: return "CLOSED — world is in Edit";
        case EInputRouteState::ClosedPaused:   return "CLOSED — play session is paused";
        }

        return "CLOSED — unknown";
    }

    void InputRoute::SetViewportFocus(const bool bInHovered, const bool bInFocused) noexcept
    {
        m_bViewportHovered = bInHovered;
        m_bViewportFocused = bInFocused;
    }

    void InputRoute::Evaluate()
    {
        const EInputRouteState lPrevious = m_State;

        // Checked in order, so the reason reported is the first thing wrong.
        const World* lActive = m_Worlds.GetActiveWorld();

        if (lActive == nullptr)
        {
            m_State = EInputRouteState::ClosedNoWorld;
        }
        else if (!m_bViewportHovered && !m_bViewportFocused)
        {
            // Hovered or focused: dragging out of the panel mid-gesture must not cut input off.
            m_State = EInputRouteState::ClosedViewport;
        }
        else if (lActive->GetMode() != EWorldMode::Play)
        {
            // Edit mode: input belongs to the editor's tools, not the engine.
            m_State = EInputRouteState::ClosedEditMode;
        }
        else if (m_PIE.IsPaused())
        {
            // A paused game must not store keys to replay on resume.
            m_State = EInputRouteState::ClosedPaused;
        }
        else
        {
            m_State = EInputRouteState::Open;
        }

        // While open, the game's pointer is the viewport-local position (the OS gives window pixels).
        // Fed once per frame.
        if (m_State == EInputRouteState::Open)
        {
            m_Input.OnMouseMoved(m_PointerLocalPx.x, m_PointerLocalPx.y);
        }

        if (m_State == lPrevious)
        {
            return;
        }

        // Not logged: the route flips every time the pointer crosses the viewport edge. The Input panel shows it.
        if (lPrevious == EInputRouteState::Open)
        {
            // Once closed, the engine no longer receives releases, so anything held now would stay held.
            m_Input.ResetState();
        }
    }
}
