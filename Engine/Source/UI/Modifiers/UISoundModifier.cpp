#include "UI/Modifiers/UISoundModifier.h"

#include "Core/Log/Logger.h"
#include "Core/Maths/Maths.h"
#include "UI/UIAudio.h"
#include "UI/UICanvas.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UISound);

    const UISounds& UISoundModifier::GetSounds() const
    {
        if (Preset.IsEmpty() || GetOwner() == nullptr || GetOwner()->GetCanvas() == nullptr
            || GetOwner()->GetCanvas()->GetAssets() == nullptr)
        {
            return Sounds;
        }

        // Read again each time: a saved preset reloads in place.
        const UISoundSet* lSet = Cast<UISoundSet>(GetOwner()->GetCanvas()->GetAssets()->ResolveAsset(Preset.Path.CStr()));
        if (lSet == nullptr)
        {
            if (m_WarnedPreset != Preset.Path)
            {
                m_WarnedPreset = Preset.Path;
                OPAAX_LOG(LogUISound, Warn, "UI Sounds '{}' did not load: '{}' plays its own sounds", Preset.Path.CStr(),
                          GetOwner()->Name.IsEmpty() ? "a widget" : GetOwner()->Name.CStr());
            }
            return Sounds;
        }
        return lSet->Sounds;
    }

    UISoundModifier& UISoundModifier::SetPreset(const OpaaxString& InAssetPath)
    {
        Preset.Path = InAssetPath;
        m_WarnedPreset.Clear();
        return *this;
    }

    void UISoundModifier::PostEditChange()
    {
        Sounds.Volume = Maths::Max(Sounds.Volume, 0.f);
        m_WarnedPreset.Clear();
        UIModifier::PostEditChange();
    }

    void UISoundModifier::Play(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty() || GetOwner() == nullptr || !bEnabled)
        {
            return;
        }

        m_LastSound = InPath;
        ++m_SoundCount;

        const UICanvas* lCanvas = GetOwner()->GetCanvas();
        if (IUIAudio* lAudio = lCanvas != nullptr ? lCanvas->GetAudio() : nullptr)
        {
            lAudio->PlaySound(InPath, GetSounds().Volume);
        }
    }

    void UISoundModifier::OnShow()
    {
        Play(GetSounds().Show);
    }

    bool UISoundModifier::OnHide()
    {
        Play(GetSounds().Hide);
        return false;   // a sound does not hold the hide
    }

    void UISoundModifier::OnActivated()
    {
        Play(GetSounds().Click);
    }

    void UISoundModifier::OnStateChanged(const EUIInteractionState InFrom, const EUIInteractionState InTo)
    {
        // Hover once per coming over: a released press (Pressed back to Hovered) is not a new hover.
        if (InTo == EUIInteractionState::Hovered && InFrom != EUIInteractionState::Pressed)
        {
            Play(GetSounds().Hover);
        }
        else if (InTo == EUIInteractionState::Pressed)
        {
            Play(GetSounds().Press);
        }
    }

    void UISoundModifier::OnReset()
    {
        m_LastSound.Clear();
        m_SoundCount = 0;
    }
}
