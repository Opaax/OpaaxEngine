#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EEventType — window and input event types. Other events go through the EventBus.
    // =============================================================================
    enum class EEventType : Uint8
    {
        None = 0,

        //--- Window ---
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,

        //--- Keyboard ---
        KeyPressed,
        KeyReleased,
        KeyTyped,

        //--- Mouse ---
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled,
    };

    // =============================================================================
    // EEventCategory — bitmask. An event can have several (a key press is Input | Keyboard).
    // =============================================================================
    enum class EEventCategory : Uint16
    {
        None        = 0,
        Application = BIT(0),   // window events
        Input       = BIT(1),   // any input
        Keyboard    = BIT(2),   // keyboard specifically
        Mouse       = BIT(3),   // mouse move / scroll
        MouseButton = BIT(4),   // mouse button press / release
    };

    // Scoped but still usable as a bitmask.
    constexpr EEventCategory operator|(EEventCategory InA, EEventCategory InB) noexcept
    {
        return static_cast<EEventCategory>(static_cast<Uint16>(InA) | static_cast<Uint16>(InB));
    }
}
