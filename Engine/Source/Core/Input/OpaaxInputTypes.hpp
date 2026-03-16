#pragma once
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    enum struct EOpaaxKeyCode : Uint16
    {
        // -----------------------------------------------------------------------------
        // None
        None = 0,

        // -----------------------------------------------------------------------------
        // Any
        AnyKey = 1,
        
        // -----------------------------------------------------------------------------
        // Printable Keys (ASCII-compatible)
        Space       = 32,
        Apostrophe  = 39,
        Comma       = 44,
        Minus       = 45,
        Period      = 46,
        Slash       = 47,

        // -----------------------------------------------------------------------------
        
        Zero    = 48, // D0
        One     = 49, // D1
        Two     = 50, // D2
        Three   = 51, // D3
        Four    = 52, // D4
        Five    = 53, // D5
        Six     = 54, // D6
        Seven   = 55, // D7
        Eight   = 56, // D8
        Nine    = 57, // D9
        
        // -----------------------------------------------------------------------------

        Semicolon   = 59,
        Equals      = 61,

        // -----------------------------------------------------------------------------
        
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,

        // -----------------------------------------------------------------------------

        LeftBracket     = 91,
        Backslash       = 92,
        RightBracket    = 93,
        GraveAccent     = 96, // Tilde/backtick ~

        // -----------------------------------------------------------------------------
        // Function Keys (GLFW/SDL-compatible)
        
        Escape      = 256,
        Enter       = 257,
        Tab         = 258,
        Backspace   = 259,
        Insert      = 260,
        Delete      = 261,

        // -----------------------------------------------------------------------------
        // Arrows
        
        Right   = 262,
        Left    = 263,
        Down    = 264,
        Up      = 265,

        // -----------------------------------------------------------------------------
        // Navigation
        
        PageUp      = 266,
        PageDown    = 267,
        Home        = 268,
        End         = 269,

        // -----------------------------------------------------------------------------
        // Modifiers
        
        CapsLock    = 280,
        ScrollLock  = 281,
        NumLock     = 282,
        PrintScreen = 283,
        Pause       = 284,

        // -----------------------------------------------------------------------------
        // Function Keys
        
        F1  = 290,
        F2  = 291,
        F3  = 292,
        F4  = 293,
        F5  = 294,
        F6  = 295,
        F7  = 296,
        F8  = 297,
        F9  = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,

        // -----------------------------------------------------------------------------
        // Modifiers
        LeftShift       = 340,
        LeftControl     = 341,
        LeftAlt         = 342,
        LeftSuper       = 343, // Windows/Command key
        RightShift      = 344,
        RightControl    = 345,
        RightAlt        = 346,
        RightSuper      = 347,

        // -----------------------------------------------------------------------------
        // Numpad
        
        Numpad_Zero     = 320,
        Numpad_One      = 321,
        Numpad_Two      = 322,
        Numpad_Three    = 323,
        Numpad_Four     = 324,
        Numpad_Five     = 325,
        Numpad_Six      = 326,
        Numpad_Seven    = 327,
        Numpad_Eight    = 328,
        Numpad_Nine     = 329,
        
        // -----------------------------------------------------------------------------

        Numpad_Add      = 330,
        Numpad_Subtract = 331,
        Numpad_Multiply = 332,
        Numpad_Divide   = 333,
        Numpad_Enter    = 334,
        Numpad_Decimal  = 335,

        // -----------------------------------------------------------------------------
        // Gamepad - Buttons
        
        Gamepad_FaceButton_Bottom   = 10000,
        Gamepad_FaceButton_Right    = 10001,
        Gamepad_FaceButton_Left     = 10002,
        Gamepad_FaceButton_Top      = 10003,
        
        // -----------------------------------------------------------------------------

        Gamepad_Special_Left    = 10004,
        Gamepad_Special_Right   = 10005,

        // -----------------------------------------------------------------------------
        
        Gamepad_LeftStick   = 10006,
        Gamepad_RightStick  = 10007,
        
        // -----------------------------------------------------------------------------

        Gamepad_LeftShoulder    = 10008,
        Gamepad_RightShoulder   = 10009,

        // -----------------------------------------------------------------------------
        
        Gamepad_DPad_Up     = 10010,
        Gamepad_DPad_Down   = 10011,
        Gamepad_DPad_Right  = 10012,
        Gamepad_DPad_Left   = 10013,

        // -----------------------------------------------------------------------------
        // Triggers
        
        Gamepad_LeftTrigger     = 10014,
        Gamepad_RightTrigger    = 10015,

        // -----------------------------------------------------------------------------
        // Axes
        
        Gamepad_LeftTriggerAxis     = 10016,
        Gamepad_RightTriggerAxis    = 10017,
        
        // -----------------------------------------------------------------------------

        Gamepad_LeftX   = 10018,
        Gamepad_LeftY   = 10019,
        Gamepad_RightX  = 10020,
        Gamepad_RightY  = 10021,

        // -----------------------------------------------------------------------------
        
        Gamepad_LeftStick_Up    = 10022,
        Gamepad_LeftStick_Down  = 10023,
        Gamepad_LeftStick_Right = 10024,
        Gamepad_LeftStick_Left  = 10025,

        // -----------------------------------------------------------------------------
        
        Gamepad_RightStick_Up       = 10026,
        Gamepad_RightStick_Down     = 10027,
        Gamepad_RightStick_Right    = 10028,
        Gamepad_RightStick_Left     = 10029,
    };
}
