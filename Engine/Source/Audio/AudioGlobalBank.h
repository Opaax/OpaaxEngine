#pragma once

#include "Audio/AudioBankHold.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"

namespace Opaax
{
    // =============================================================================
    // AudioGlobalBank — holds the global audio bank (Audio.GlobalBank in Engine.config) for the
    //   whole game: the sounds of menus, the player and the UI stay loaded across level changes.
    // =============================================================================
    class AudioGlobalBank final : public GameInstanceSubsystem
    {
        OPAAX_OBJECT(AudioGlobalBank, GameInstanceSubsystem)

        //~Begin Subsystem interface
        bool Startup() override;
        void Shutdown() override;
        //~End Subsystem interface

        const AudioBankHold& GetHold() const noexcept { return m_Hold; }

    private:
        AudioBankHold m_Hold;
    };
}
