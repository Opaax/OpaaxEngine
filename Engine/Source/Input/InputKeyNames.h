#pragma once


#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Input/InputCodes.h"
#include "Input/InputManager.h"   // KEY_STATE_COUNT

namespace Opaax
{
    // =============================================================================
    // InputKeyNames — EKeyCode to text and back, for .opaaxinputmap files.
    //   Generated from InputKeyCodeList.h.
    // =============================================================================

    /** Enum to string. */
    inline const char* ToString(const EKeyCode InKey) noexcept
    {
        switch (InKey)
        {
        #define OPAAX_KEY_CODE(Name) case EKeyCode::Name: return #Name;
        #include "Input/InputKeyCodeList.h"
        #undef OPAAX_KEY_CODE
        }

        return "None";
    }

    // Parsing goes through OpaaxEnumJson.h (EKeyCode declares its values below).

    /**
     * Whether InKey can be bound: keyboard and mouse yes, gamepad not yet.
     */
    inline bool IsKeyCodeBindable(const EKeyCode InKey) noexcept
    {
        return InKey != EKeyCode::None
            && InKey != EKeyCode::AnyKey
            && static_cast<Uint16>(InKey) < InputManager::KEY_STATE_COUNT;
    }

    // =============================================================================
    // Value list, for editor dropdowns and saving by name. Written out by hand
    // (a #include cannot go inside OPAAX_ENUM_VALUES).
    // =============================================================================
    template<>
    struct TEnumValues<EKeyCode>
    {
        static constexpr EKeyCode Values[] =
        {
            #define OPAAX_KEY_CODE(Name) EKeyCode::Name,
            #include "Input/InputKeyCodeList.h"
            #undef OPAAX_KEY_CODE
        };
    };
}
