#include "Audio/AudioBankHold.h"

#include "Application/Services/IPaths.h"
#include "Audio/AudioTypes.h"
#include "Resources/ResourceManager.h"

namespace Opaax
{
    bool AudioBankHold::Hold(const OpaaxString& InBankPath, ResourceManager& InResources, const IPaths& InPaths)
    {
        TDataAssetRef<AudioBank> lRef;
        lRef.Path = InBankPath;

        TDataAssetHandle<AudioBank> lBank = LoadDataAsset(InResources, InPaths, lRef);
        if (lBank.Get() == nullptr)
        {
            OPAAX_LOG(LogAudio, Warn, "Audio bank '{}' does not load: its sounds load when they first play", InBankPath.CStr());
            return false;
        }

        Hold(*lBank.Get(), InResources, InPaths);
        m_Banks.push_back(Move(lBank));
        return true;
    }

    void AudioBankHold::Hold(const AudioBank& InBank, ResourceManager& InResources, const IPaths& InPaths)
    {
        for (const TDataAssetRef<AudioClipDef>& lDefRef : InBank.ClipDefs)
        {
            TDataAssetHandle<AudioClipDef> lDef = LoadDataAsset(InResources, InPaths, lDefRef);
            if (lDef.Get() == nullptr)
            {
                OPAAX_LOG(LogAudio, Warn, "A bank names clip definition '{}', which does not load", lDefRef.Path.CStr());
                continue;
            }

            for (const AudioClipEntry& lEntry : lDef.Get()->Clips)
            {
                HoldClip(lEntry.Clip.Path, InResources, InPaths);
            }
            m_Defs.push_back(Move(lDef));
        }

        for (const TResourcePath<AudioClipResource>& lClip : InBank.Clips)
        {
            HoldClip(lClip.Path, InResources, InPaths);
        }
    }

    void AudioBankHold::Release()
    {
        m_Clips.clear();
        m_Defs.clear();
        m_Banks.clear();
    }

    const ResourceRef<AudioClipResource>* AudioBankHold::FindClip(const OpaaxString& InPath) const
    {
        const auto lFound = m_Clips.find(InPath);
        return lFound != m_Clips.end() ? &lFound->second : nullptr;
    }

    void AudioBankHold::HoldClip(const OpaaxString& InPath, ResourceManager& InResources, const IPaths& InPaths)
    {
        if (InPath.IsEmpty() || m_Clips.contains(InPath))
        {
            return;
        }

        const OpaaxString lAbsolute = InPaths.AssetToAbsolute(InPath);
        m_Clips.emplace(InPath, InResources.LoadAsync<AudioClipResource>(lAbsolute.CStr()));
    }
}
