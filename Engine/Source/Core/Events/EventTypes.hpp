#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EEventType — Tier-1 dispatched events
    //
    // The runtime tag every Tier-1 Event carries; EventDispatcher matches on it
    // (enum compare, no RTTI). Tier-1 is OS/window/input ONLY. Domain events
    // (physics, editor, gameplay) are Tier-3 bus payloads — POD structs keyed by a
    // hashed type-name — and deliberately do NOT get an entry here.
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
    // EEventCategory — bitmask flags
    //
    // A single event may belong to several categories (e.g. a key press is both
    // Input and Keyboard). Unscoped on purpose so the flags OR together as ints.
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

    // Scoped (enumerators don't leak into the namespace) but still bitmaskable — an
    // event combines flags, e.g. EEventCategory::Input | EEventCategory::Keyboard.
    constexpr EEventCategory operator|(EEventCategory InA, EEventCategory InB) noexcept
    {
        return static_cast<EEventCategory>(static_cast<Uint16>(InA) | static_cast<Uint16>(InB));
    }
}
