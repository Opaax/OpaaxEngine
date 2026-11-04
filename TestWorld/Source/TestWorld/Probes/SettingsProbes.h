#pragma once

#include "Application/Services/IUserSettings.h"
#include "Audio/AudioManager.h"
#include "Engine/Config/EngineConfigData.h"
#include "Input/InputKeyNames.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"

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

    // =============================================================================
    // RebindProbe — what a controls menu does, from gameplay: rebinds Fire (F in Input/Test.opaaxasset,
    //   which it adds) to the key RebindTo names, and counts Fire's presses. Records the key Fire has and
    //   what the last rebind did.
    // =============================================================================
    class RebindProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(RebindProbe, Opaax::Behaviour)

    public:
        /** Set by a test: the next frame rebinds Fire to the key it names ("None" clears it), then it empties. */
        Opaax::OpaaxString RebindTo;

        /** What the last rebind did ("Rebound", "InvalidKey"...). */
        Opaax::OpaaxString Result;

        /** The key Fire has, as the player's keys say. */
        Opaax::OpaaxString FireKey;

        Opaax::Int32 Fires = 0;

        OPAAX_PROPERTIES(RebindProbe, OPAAX_PROP(RebindTo), OPAAX_PROP(Result), OPAAX_PROP(FireKey), OPAAX_PROP(Fires))

        void OnStart() override
        {
            if (Opaax::InputMappingSubsystem* lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("Test"), Opaax::OpaaxString("Input/Test.opaaxasset"));
            }
            BindAction<&RebindProbe::OnFire>(OPAAX_ID("Fire"), Opaax::EInputTrigger::Started);
        }

        void OnUpdate(float) override
        {
            Opaax::InputMappingSubsystem* lActions = GetContext().Actions;
            if (lActions == nullptr)
            {
                return;
            }

            const Opaax::InputKeySlot lFire{ Opaax::OpaaxString("Input/Test.opaaxasset"), OPAAX_ID("Fire"),
                                             Opaax::EKeyCode::F };
            if (!RebindTo.IsEmpty())
            {
                Opaax::EKeyCode lKey = Opaax::EKeyCode::None;
                Result = Opaax::KeyFromName(RebindTo, lKey)
                       ? Opaax::OpaaxString(ToString(lActions->Rebind(lFire, lKey).Result))
                       : Opaax::OpaaxString("no such key");
                RebindTo = Opaax::OpaaxString();
            }

            FireKey = Opaax::OpaaxString(ToString(lActions->GetKey(lFire)));
        }

        void OnFire(const Opaax::InputActionValue&) { ++Fires; }
    };
}
