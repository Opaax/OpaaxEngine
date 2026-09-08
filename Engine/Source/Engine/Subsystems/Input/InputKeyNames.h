#pragma once


#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/Subsystems/Input/InputManager.h"   // KEY_STATE_COUNT — the bindable bound

namespace Opaax
{
    // =============================================================================
    // InputKeyNames — EKeyCode as TEXT, both directions.
    //
    //   WHY IT EXISTS: a `.opaaxinputmap` stores which key drives an action, and this engine
    //   writes LABELS rather than ordinals everywhere else (OpaaxEnumJson.h). "A" survives a
    //   human editing the file; 65 does not survive being read by one.
    //
    //   Generated from InputKeyCodeList.h, so ToString and the value list cannot disagree —
    //   the CollisionChannel.h shape, for its reason.
    //
    //   NOT the editor's KeyName (InputPanel.cpp). That one is a DISPLAY helper: it abbreviates
    //   ("Esc", "LMB"), covers a handful of cases and falls through for the rest. Abbreviations
    //   are good in a panel and wrong in a file format, so these stay separate — this is the
    //   canonical spelling, that one is the pretty one.
    // =============================================================================

    /** I11: the mapping lives with the enum, found by ADL. */
    inline const char* ToString(const EKeyCode InKey) noexcept
    {
        switch (InKey)
        {
        #define OPAAX_KEY_CODE(Name) case EKeyCode::Name: return #Name;
        #include "Engine/Subsystems/Input/InputKeyCodeList.h"
        #undef OPAAX_KEY_CODE
        }

        return "None";
    }

    // NOTE: there is deliberately no KeyCodeFromString here. OpaaxEnumJson.h's from_json already
    // parses ANY CEnumWithValues by scanning its value list for a matching ToString, and the
    // TEnumValues specialisation below is what makes EKeyCode one of those. A second parser would
    // be a second answer to one question.

    /**
     * Whether InKey has a FEED behind it, and so can be bound.
     *
     * Keyboard and mouse do; the gamepad range is reserved in EKeyCode but GLFW exposes pads by
     * POLLING — a second feed that does not exist yet (IN7). A binding to one is refused loudly
     * rather than accepted and silently dead, and this is the one place that decides.
     *
     * KEY_STATE_COUNT is the authority rather than a second magic number: it is exactly the range
     * InputManager keeps state for, so "bindable" and "readable" cannot drift apart.
     */
    inline bool IsKeyCodeBindable(const EKeyCode InKey) noexcept
    {
        return InKey != EKeyCode::None
            && InKey != EKeyCode::AnyKey
            && static_cast<Uint16>(InKey) < InputManager::KEY_STATE_COUNT;
    }

    // =============================================================================
    // The value list, so an EKeyCode field draws as a dropdown and serializes by label with no
    // per-type editor code. Written out rather than stamped with OPAAX_ENUM_VALUES for
    // CollisionChannel.h's reason: the list lives in an #include, and a preprocessor directive
    // cannot appear inside a macro argument.
    // =============================================================================
    template<>
    struct TEnumValues<EKeyCode>
    {
        static constexpr EKeyCode Values[] =
        {
            #define OPAAX_KEY_CODE(Name) EKeyCode::Name,
            #include "Engine/Subsystems/Input/InputKeyCodeList.h"
            #undef OPAAX_KEY_CODE
        };
    };
}
