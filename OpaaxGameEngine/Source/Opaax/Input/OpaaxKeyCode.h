#pragma once
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace INPUT
    {
        enum struct EOPKeyCode : UInt32
        {
            // ----- None -----
            None                = 0x0000'0000,

            // ----- Any -----
            AnyKey              = 0x0000'0001,

            // ----- Letters -----
            A                   = 0x0000'0002,
            B                   = 0x0000'0003,
            C                   = 0x0000'0004,
            D                   = 0x0000'0005,
            E                   = 0x0000'0006,
            F                   = 0x0000'0007,
            G                   = 0x0000'0008,
            H                   = 0x0000'0009,
            I                   = 0x0000'000A,
            J                   = 0x0000'000B,
            K                   = 0x0000'000C,
            L                   = 0x0000'000D,
            M                   = 0x0000'000E,
            N                   = 0x0000'000F,
            O                   = 0x0000'0010,
            P                   = 0x0000'0011,
            Q                   = 0x0000'0012,
            R                   = 0x0000'0013,
            S                   = 0x0000'0014,
            T                   = 0x0000'0015,
            U                   = 0x0000'0016,
            V                   = 0x0000'0017,
            W                   = 0x0000'0018,
            X                   = 0x0000'0019,
            Y                   = 0x0000'001A,
            Z                   = 0x0000'001B,

            // ----- Numbers (Top Row) -----
            Zero                = 0x0000'001C,
            One                 = 0x0000'001D,
            Two                 = 0x0000'001E,
            Three               = 0x0000'001F,
            Four                = 0x0000'0020,
            Five                = 0x0000'0021,
            Six                 = 0x0000'0022,
            Seven               = 0x0000'0023,
            Eight               = 0x0000'0024,
            Nine                = 0x0000'0025,

            // ----- Numpad -----
            Numpad_Zero         = 0x0000'0026,
            Numpad_One          = 0x0000'0027,
            Numpad_Two          = 0x0000'0028,
            Numpad_Three        = 0x0000'0029,
            Numpad_Four         = 0x0000'002A,
            Numpad_Five         = 0x0000'002B,
            Numpad_Six          = 0x0000'002C,
            Numpad_Seven        = 0x0000'002D,
            Numpad_Nine         = 0x0000'002F,
            Numpad_Eight        = 0x0000'002E,

            Numpad_Add          = 0x0000'0030,
            Numpad_Subtract     = 0x0000'0031,
            Numpad_Multiply     = 0x0000'0032,
            Numpad_Divide       = 0x0000'0033,
            Numpad_Enter        = 0x0000'0034,
            Numpad_Decimal      = 0x0000'0035,

            // ----- Arrows -----
            Left                = 0x0000'0036,
            Up                  = 0x0000'0037,
            Right               = 0x0000'0038,
            Down                = 0x0000'0039,

            // ----- Function Keys -----
            F1                  = 0x0000'003A,
            F2                  = 0x0000'003B,
            F3                  = 0x0000'003C,
            F4                  = 0x0000'003D,
            F5                  = 0x0000'003E,
            F6                  = 0x0000'003F,
            F7                  = 0x0000'0040,
            F8                  = 0x0000'0041,
            F9                  = 0x0000'0042,
            F10                 = 0x0000'0043,
            F11                 = 0x0000'0044,
            F12                 = 0x0000'0045,

            // ----- Special Characters -----
            Semicolon           = 0x0000'0046,
            Apostrophe          = 0x0000'0047,
            Comma               = 0x0000'0048,
            Period              = 0x0000'0049,
            Slash               = 0x0000'004A,
            Backslash           = 0x0000'004B,
            LeftBracket         = 0x0000'004C,
            RightBracket        = 0x0000'004D,
            Minus               = 0x0000'004E,
            Equals              = 0x0000'004F,
            Grave               = 0x0000'0050, // Tilde/backtick

            // ----- Modifiers -----
            LeftShift           = 0x0000'0051,
            RightShift          = 0x0000'0052,
            LeftCtrl            = 0x0000'0053,
            RightCtrl           = 0x0000'0054,
            LeftAlt             = 0x0000'0055,
            RightAlt            = 0x0000'0056,

            // ----- Exec -----
            CapsLock            = 0x0000'0057,
            Tab                 = 0x0000'0058,
            Enter               = 0x0000'0059,
            Escape              = 0x0000'005A,
            Space               = 0x0000'005B,
            Backspace           = 0x0000'005C,
            Insert              = 0x0000'005D,
            Delete              = 0x0000'005E,
            Home                = 0x0000'005F,
            End                 = 0x0000'0060,
            PageUp              = 0x0000'0061,
            PageDown            = 0x0000'0062,
            PrintScreen         = 0x0000'0063,
            ScrollLock          = 0x0000'0064,
            Pause               = 0x0000'0065,

            // ----- Others -----
            //.....

            // ----- Gamepad Buttons -----
            Gamepad_FaceButton_Bottom   = 0x0001'0000,
            Gamepad_FaceButton_Right    = 0x0001'0001,
            Gamepad_FaceButton_Left     = 0x0001'0002,
            Gamepad_FaceButton_Top      = 0x0001'0003,

            Gamepad_Special_Left        = 0x0001'0004,
            Gamepad_Special_Right       = 0x0001'0005,

            Gamepad_LeftStick           = 0x0001'0006,
            Gamepad_RightStick          = 0x0001'0007,
            
            Gamepad_LeftShoulder        = 0x0001'0008,
            Gamepad_RightShoulder       = 0x0001'0009,

            Gamepad_DPad_Up             = 0x0001'000A,
            Gamepad_DPad_Down           = 0x0001'000B,
            Gamepad_DPad_Right          = 0x0001'000C,
            Gamepad_DPad_Left           = 0x0001'000D,

            // ----- Gamepad Triggers -----
            Gamepad_LeftTrigger         = 0x0001'000E,
            Gamepad_RightTrigger        = 0x0001'000F,

            Gamepad_LeftTriggerAxis     = 0x0001'0010,
            Gamepad_RightTriggerAxis    = 0x0001'0011,

            // ----- Gamepad Axes (Optional) -----
            Gamepad_LeftX               = 0x0001'0012,
            Gamepad_LeftY               = 0x0001'0013,
            Gamepad_RightX              = 0x0001'0014,
            Gamepad_RightY              = 0x0001'0015,

            Gamepad_LeftStick_Up        = 0x0001'0015,
            Gamepad_LeftStick_Down      = 0x0001'0016,
            Gamepad_LeftStick_Right     = 0x0001'0017,
            Gamepad_LeftStick_Left      = 0x0001'0018,

            Gamepad_RightStick_Up       = 0x0001'0019,
            Gamepad_RightStick_Down     = 0x0001'001A,
            Gamepad_RightStick_Right    = 0x0001'001B, 
            Gamepad_RightStick_Left     = 0x0001'001C,
            
            Gamepad_LeftStick_Key_Down  = 0x0001'001C, //thumb stick pressed
            Gamepad_LeftStick_Key_Up    = 0x0001'001C, //thumb stick release
            Gamepad_RightStick_Key_Down = 0x0001'001C,  //thumb stick release
            Gamepad_RightStick_Key_Up   = 0x0001'001C,  //thumb stick pressed
        };
    }
}
