#pragma once

#include "Application/Services/IUserSettings.h"
#include "Audio/AudioManager.h"
#include "Engine/Config/EngineConfigData.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // SettingsProbe — what a settings menu does, from gameplay: sets Volume (when 0 or more) as the
    //   player's master volume through GetUserSettings. Records each frame whether the settings are over
    //   the configs, the master volume the engine config reads and the one the mixer plays at.
    // =============================================================================
    class SettingsProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(SettingsProbe, Opaax::Behaviour)

    public:
        /** Set by a test: the next frame sets it as the player's master volume, then it goes back to -1. */
        float Volume = -1.f;

        /** The last volume set was taken. */
        bool bTaken = false;

        /** The player's settings are over the configs. */
        bool bActive = false;

        /** The master volume in what the engine reads (GetContext().Config). */
        float ConfigVolume = 0.f;

        /** The master volume the mixer plays at. */
        float MixerVolume = 0.f;

        /** The Music bus's volume in the mixer. */
        float MusicVolume = 0.f;

        OPAAX_PROPERTIES(SettingsProbe, OPAAX_PROP(Volume), OPAAX_PROP(bTaken), OPAAX_PROP(bActive),
                         OPAAX_PROP(ConfigVolume), OPAAX_PROP(MixerVolume), OPAAX_PROP(MusicVolume))

        void OnUpdate(float) override
        {
            Opaax::IUserSettings& lSettings = GetUserSettings();
            if (Volume >= 0.f)
            {
                bTaken = lSettings.SetMasterVolume(Volume);
                Volume = -1.f;
            }

            bActive      = lSettings.IsActive();
            ConfigVolume = GetContext().Config.Audio.MasterVolume;

            if (const Opaax::AudioManager* const lAudio = GetContext().Audio)
            {
                MixerVolume = lAudio->GetMasterVolume();
                MusicVolume = lAudio->GetBusVolume(OPAAX_ID("Music"));
            }
        }
    };
}
