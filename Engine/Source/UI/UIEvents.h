#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // Opaque declaration (fixed underlying type), so this module does not include the engine input headers.
    enum class EKeyCode : Uint16;

    // =============================================================================
    // UI events — events bubble: the hit widget first, then its parents, until one returns Handled.
    //   Unhandled events go through to the game.
    // =============================================================================
    enum class EUIReply : Uint8
    {
        Unhandled,
        Handled
    };

    /** UI pointer buttons (the host maps physical codes to them). */
    enum class EUIPointerButton : Uint8
    {
        None,
        Primary,
        Secondary,
        Middle
    };

    enum class EUIPointerEventType : Uint8
    {
        Move,    // sent to the hovered widget, not bubbled or handled
        Enter,   // sent, not bubbled
        Leave,   // sent, not bubbled
        Down,    // bubbled; the handler captures the pointer
        Up       // bubbled from the capturing widget, else the hit one
    };

    struct UIPointerEvent
    {
        EUIPointerEventType Type     = EUIPointerEventType::Move;
        Vector2F            Position = { 0.f, 0.f };   // canvas units
        EUIPointerButton    Button   = EUIPointerButton::None;
    };

    /** Bubbled from the focused widget. */
    struct UIKeyEvent
    {
        EKeyCode Key      = static_cast<EKeyCode>(0);   // None
        bool     bPressed = true;                       // false = released
    };
}
