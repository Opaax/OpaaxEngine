#pragma once

#include "Audio/AudioBankHold.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    struct GameInstanceContext;

    // =============================================================================
    // AudioGlobalBank — holds the global audio bank (Audio.GlobalBank in Engine.config) for the
    //   whole game: the sounds of menus, the player and the UI stay loaded across level changes.
    // =============================================================================
    class AudioGlobalBank final : public GameInstanceSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(AudioGlobalBank)

        explicit AudioGlobalBank(GameInstanceContext& InContext);

        //~Begin ISubsystem interface
        bool Startup() override;
        void Shutdown() override;
        //~End ISubsystem interface

        const AudioBankHold& GetHold() const noexcept { return m_Hold; }

    private:
        GameInstanceContext* m_Context = nullptr;
        AudioBankHold        m_Hold;
    };
}
