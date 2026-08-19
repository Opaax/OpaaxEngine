#include "Editor/Input/InputRoute.h"

#include "Editor/PIE/PlayInEditor.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Engine/Subsystems/Input/InputManager.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogInputRoute{"InputRoute"};
}

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

        // D5's order, so the reason reported is the FIRST thing that is wrong rather than an
        // arbitrary one of several.
        const World* lActive = m_Worlds.GetActiveWorld();

        if (lActive == nullptr)
        {
            m_State = EInputRouteState::ClosedNoWorld;
        }
        else if (!m_bViewportHovered && !m_bViewportFocused)
        {
            // Step 2. Hovered OR focused: dragging out of the panel mid-gesture must not cut the
            // input off, and a click-to-focus play session must survive the pointer wandering.
            m_State = EInputRouteState::ClosedViewport;
        }
        else if (lActive->GetMode() != EWorldMode::Play)
        {
            // Step 4. Edit belongs to the editor's own tools. There are none yet, so the input
            // stops here — which is correct, not a gap.
            m_State = EInputRouteState::ClosedEditMode;
        }
        else if (m_PIE.IsPaused())
        {
            // A frozen world must not bank keystrokes to replay on resume.
            m_State = EInputRouteState::ClosedPaused;
        }
        else
        {
            m_State = EInputRouteState::Open;
        }

        if (m_State == lPrevious)
        {
            return;
        }

        // A transition, so this logs once per change rather than every frame.
        OPAAX_LOG(LogInputRoute, Info, "Input route {}", ToString(m_State));

        if (lPrevious == EInputRouteState::Open)
        {
            // THE reason this object holds state. The engine stops being told about releases the
            // instant the route closes, so anything held now would stay held forever.
            m_Input.ResetState();
        }
    }
}
