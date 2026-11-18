#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // IUIAudio — plays a UI sound for widgets, so the UI module never touches the mixer. A canvas holds
    //   one, set by its host (the game's UI subsystem, the editor's preview); none is silence.
    // =============================================================================
    class IUIAudio
    {
    public:
        virtual ~IUIAudio() = default;

        /**
         * Plays a sound file ("Audio/Click.wav") or a clip definition ("Audio/Clicks.opaaxasset") once,
         * on the UI bus, so a pause menu is heard while the world is paused.
         * @param InVolume Linear, multiplied with a definition's own
         */
        virtual void PlaySound(const OpaaxString& InPath, float InVolume) = 0;
    };
}
