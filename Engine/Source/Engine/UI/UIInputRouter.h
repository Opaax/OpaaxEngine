#pragma once

#include "Core/EngineAPI.h"
#include "Engine/Input/InputActionEvaluator.h"   // InputKeyMask
#include "Engine/UI/UIInputMode.h"

namespace Opaax
{
    class InputManager;
    class UICanvas;

    // =============================================================================
    // UIInputRouter — ONE frame of UI input, pure and free of the game. It reads the raw
    //   InputManager, drives the canvas's pointer/key routing, and reports which keys the UI
    //   swallowed so the mapping can skip them (UI10). Hoisted out of UISubsystem for the same
    //   reason InputActionEvaluator is out of InputMappingSubsystem: it is testable with a real
    //   InputManager and no game, no world, no GL.
    // =============================================================================
    namespace UIInputRouter
    {
        /**
         * Route this frame per the mode.
         *  - GameOnly:  the canvas's pointer is cleared, nothing routed, nothing consumed.
         *  - GameAndUI: pointer + keys routed; whatever a widget handled is marked in OutConsumed.
         *  - UIOnly:    routed, and OutConsumed is filled WHOLE — the mapping is muted.
         *
         * @param OutConsumed Zeroed then filled. Hand it to InputMappingSubsystem::ConsumeThisFrame.
         */
        OPAAX_API void Route(const InputManager& InInput, UICanvas& InCanvas, EUIInputMode InMode,
                             InputKeyMask& OutConsumed);
    }
}
