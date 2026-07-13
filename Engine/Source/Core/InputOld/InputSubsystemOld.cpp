#include "InputSubsystemOld.h"

#include "Core/EventOld/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool InputSubsystemOld::HandleKeyPressed(const KeyPressedEvent& Event)
    {
        SetState(Event.GetKeyCode(), INPUT_STATE_PRESSED);
        return false;
    }
 
    bool InputSubsystemOld::HandleKeyReleased(const KeyReleasedEvent& Event)
    {
        SetState(Event.GetKeyCode(), INPUT_STATE_RELEASED);
        return false;
    }
 
    bool InputSubsystemOld::HandleMouseButtonPressed(const MouseButtonPressedEvent& Event)
    {
        SetState(Event.GetMouseButton(), INPUT_STATE_PRESSED);
        return false;
    }
 
    bool InputSubsystemOld::HandleMouseButtonReleased(const MouseButtonReleasedEvent& Event)
    {
        SetState(Event.GetMouseButton(), INPUT_STATE_RELEASED);
        return false;
    }
 
    bool InputSubsystemOld::HandleMouseMoved(MouseMovedEvent& Event)
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

    bool InputSubsystemOld::Startup()
    {
        OPAAX_CORE_INFO("InputSubsystem::Startup()");
 
        m_Current.fill(INPUT_STATE_RELEASED);
        m_Previous.fill(INPUT_STATE_RELEASED);
        m_MouseX = m_MouseY = 0.f;
        m_MouseDeltaX = m_MouseDeltaY = 0.f;
        m_bMouseMoved = false;
 
        return true;
    }

    void InputSubsystemOld::Shutdown()
    {
        OPAAX_CORE_INFO("InputSubsystem::Shutdown()");
    }

    void InputSubsystemOld::Update(double DeltaTime)
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

    bool InputSubsystemOld::OnEvent(OpaaxEvent& Event)
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

    Uint32 InputSubsystemOld::GetEventCategoryFilter() const noexcept
    {
        return EEventCategory_Input
            | EEventCategory_Keyboard
            | EEventCategory_Mouse
            | EEventCategory_MouseButton;
    }

}
