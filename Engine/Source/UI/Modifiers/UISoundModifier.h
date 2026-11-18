#pragma once

#include "Resources/Asset/AssetRef.h"
#include "UI/Sounds/UISoundSet.h"   // complete: a modifier class visits its preset reference
#include "UI/UIModifier.h"

namespace Opaax
{
    // =============================================================================
    // UISoundModifier — plays its widget's sounds through the canvas's audio (IUIAudio): on hover, press,
    //   click (the Activate stage), show and hide. On a widget following a button, its button's hover
    //   and click. A disabled button is never hovered, so it is silent. A Preset (a UISoundSet asset)
    //   wins over its own Sounds when it names one that loads.
    // =============================================================================
    class UISoundModifier final : public UIModifier
    {
        OPAAX_OBJECT(UISoundModifier, UIModifier)

    public:
        /** Shared sounds: when set and loaded, used instead of Sounds. */
        TAssetRef<UISoundSet> Preset;
        UISounds              Sounds;

        OPAAX_PROPERTIES(UISoundModifier,
                         OPAAX_PROP(Preset).SetTooltip("A UI Sounds asset. When set, its sounds are used\n"
                                                       "instead of the ones below."),
                         OPAAX_PROP(Sounds).SetTooltip("What plays on hover, press, click, show and hide."))

        /** The sounds in use: the Preset's through the owner's canvas, else Sounds (one warning). */
        const UISounds& GetSounds() const;

        /** The last sound it asked for (empty before any) and how many it asked for: what a script checks. */
        const OpaaxString& GetLastSound() const noexcept { return m_LastSound; }
        Uint32             GetSoundCount() const noexcept { return m_SoundCount; }

        // Setters that chain: lWidget->AddModifier<UISoundModifier>().SetPreset(...)
        UISoundModifier& SetPreset(const OpaaxString& InAssetPath);

        //~Begin Object interface
        void PostEditChange() override;
        //~End Object interface

    protected:
        //~Begin UIModifier interface
        void OnShow() override;
        bool OnHide() override;
        void OnActivated() override;
        void OnStateChanged(EUIInteractionState InFrom, EUIInteractionState InTo) override;
        void OnReset() override;
        //~End UIModifier interface

    private:
        /** Asks the canvas's audio for InPath (nothing when empty, or without audio). */
        void Play(const OpaaxString& InPath);

        OpaaxString m_LastSound;
        Uint32      m_SoundCount = 0;

        /** The preset path already warned about, so a missing one is said once. */
        mutable OpaaxString m_WarnedPreset;
    };
}
