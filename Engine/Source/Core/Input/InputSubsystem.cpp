#include "InputSubsystem.h"

#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool InputSubsystem::HandleKeyPressed(const KeyPressedEvent& Event)
    {
        SetState(Event.GetKeyCode(), INPUT_STATE_PRESSED);
        return false;
    }
 
    bool InputSubsystem::HandleKeyReleased(const KeyReleasedEvent& Event)
    {
        SetState(Event.GetKeyCode(), INPUT_STATE_RELEASED);
        return false;
    }
 
    bool InputSubsystem::HandleMouseButtonPressed(const MouseButtonPressedEvent& Event)
    {
        SetState(Event.GetMouseButton(), INPUT_STATE_PRESSED);
        return false;
    }
 
    bool InputSubsystem::HandleMouseButtonReleased(const MouseButtonReleasedEvent& Event)
    {
        SetState(Event.GetMouseButton(), INPUT_STATE_RELEASED);
        return false;
    }
 
    bool InputSubsystem::HandleMouseMoved(MouseMovedEvent& Event)
    {
        const float lNewX = Event.GetX();
        const float lNewY = Event.GetY();
 
        m_MouseDeltaX = lNewX - m_MouseX;
        m_MouseDeltaY = lNewY - m_MouseY;
        m_MouseX      = lNewX;
        m_MouseY      = lNewY;
        m_bMouseMoved = true;
 
        return false;
    }

    bool InputSubsystem::Startup()
    {
        OPAAX_CORE_INFO("InputSubsystem::Startup()");
 
        m_Current.fill(INPUT_STATE_RELEASED);
        m_Previous.fill(INPUT_STATE_RELEASED);
        m_MouseX = m_MouseY = 0.f;
        m_MouseDeltaX = m_MouseDeltaY = 0.f;
        m_bMouseMoved = false;
 
        return true;
    }

    void InputSubsystem::Shutdown()
    {
        OPAAX_CORE_INFO("InputSubsystem::Shutdown()");
    }

    void InputSubsystem::Update(double DeltaTime)
    {
        // Snapshot current → previous.
        // Order in the game loop:
        //   PollEvents() → DispatchEventAll() → OnEvent() updates m_Current
        //   → UpdateAll() → Update() snapshots m_Current into m_Previous
        //   → game queries IsKeyJustPressed() — compares this frame vs last
        m_Previous = m_Current;
 
        if (!m_bMouseMoved)
        {
            m_MouseDeltaX = 0.f;
            m_MouseDeltaY = 0.f;
        }
        m_bMouseMoved = false;
    }

    bool InputSubsystem::OnEvent(OpaaxEvent& Event)
    {
        OpaaxEventDispatcher lDispatcher(Event);
 
        lDispatcher.Dispatch<KeyPressedEvent>         ([this](KeyPressedEvent& Event)          { return HandleKeyPressed(Event);          });
        lDispatcher.Dispatch<KeyReleasedEvent>        ([this](KeyReleasedEvent& Event)         { return HandleKeyReleased(Event);         });
        lDispatcher.Dispatch<MouseButtonPressedEvent> ([this](MouseButtonPressedEvent& Event)  { return HandleMouseButtonPressed(Event);  });
        lDispatcher.Dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& Event) { return HandleMouseButtonReleased(Event); });
        lDispatcher.Dispatch<MouseMovedEvent>         ([this](MouseMovedEvent& Event)          { return HandleMouseMoved(Event);          });
 
        //Never consume — game code still receives all events via OnEvent.
        return false;
    }

    Uint32 InputSubsystem::GetEventCategoryFilter() const noexcept
    {
        return EEventCategory_Input
            | EEventCategory_Keyboard
            | EEventCategory_Mouse
            | EEventCategory_MouseButton;
    }

}
