#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // OPAQUE declaration: the real enum (Engine/Subsystems/Input/InputCodes.h) has a fixed
    // underlying type, so this module can carry a key without naming the Engine tier.
    enum class EKeyCode : Uint16;

    // =============================================================================
    // UI events — what a widget is asked, and its answer. Events BUBBLE: the hit widget is asked
    //   first, then its parent, up to the root, until one answers Handled (Slate's FReply).
    //   A widget with no opinion returns Unhandled and the event falls through to the game.
    // =============================================================================
    enum class EUIReply : Uint8
    {
        Unhandled,
        Handled
    };

    /** The UI's own vocabulary for pointer buttons; the host maps physical codes onto it. */
    enum class EUIPointerButton : Uint8
    {
        None,
        Primary,
        Secondary,
        Middle
    };

    enum class EUIPointerEventType : Uint8
    {
        Move,    // delivered to the hovered widget, never bubbled, never handled
        Enter,   // delivered, not bubbled
        Leave,   // delivered, not bubbled
        Down,    // bubbled; the handler captures the pointer
        Up       // bubbled from the capturing widget if any, else the hit
    };

    struct UIPointerEvent
    {
        EUIPointerEventType Type     = EUIPointerEventType::Move;
        Vector2F            Position = { 0.f, 0.f };   // canvas units
        EUIPointerButton    Button   = EUIPointerButton::None;
    };

    /** Bubbled from the FOCUSED widget. */
    struct UIKeyEvent
    {
        EKeyCode Key      = static_cast<EKeyCode>(0);   // the real enum's None
        bool     bPressed = true;                       // false = released
    };
}
