#pragma once

#include "Core/EngineAPI.h"
#include "Engine/Input/InputActionEvaluator.h"   // InputKeyMask
#include "Engine/UI/UIInputMode.h"

namespace Opaax
{
    class InputManager;
    class UICanvas;

    // =============================================================================
    // UIInputRouter — routes one frame of raw input to the UI canvas and reports which keys
    //   the UI used, so input mapping skips them. Testable without a game or GL.
    // =============================================================================
    namespace UIInputRouter
    {
        /**
         * Routes this frame according to the mode:
         *  - GameOnly:  nothing routed or consumed.
         *  - GameAndUI: pointer and keys routed; what a widget handled is marked in OutConsumed.
         *  - UIOnly:    routed, and every key is marked in OutConsumed.
         * @param OutConsumed Pass it to InputMappingSubsystem::ConsumeThisFrame
         */
        OPAAX_API void Route(const InputManager& InInput, UICanvas& InCanvas, EUIInputMode InMode,
                             InputKeyMask& OutConsumed);
    }
}
