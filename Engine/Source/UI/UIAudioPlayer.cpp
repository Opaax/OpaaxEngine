#include "UI/UIAudioPlayer.h"

#include "Application/Services/IPaths.h"
#include "Audio/AudioManager.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxPathString.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/ResourceManager.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UIAudio);

    namespace
    {
        const OpaaxTag UI_BUS("Audio.Bus.UI");

        bool IsClipDefPath(const OpaaxString& InPath)
        {
            return PathString::Extension(OpaaxStringView(InPath.CStr(), InPath.GetLength()))
                   == OpaaxStringView(AssetFile::ASSET_EXTENSION);
        }
    }

    UIAudioPlayer::UIAudioPlayer(AudioManager& InAudio, ResourceManager& InResources, const IPaths& InPaths)
        : m_Audio(InAudio)
        , m_Resources(InResources)
        , m_Paths(InPaths)
    {
    }

    void UIAudioPlayer::PlaySound(const OpaaxString& InPath, const float InVolume)
    {
        if (InPath.IsEmpty() || !m_Audio.IsStarted())
        {
            return;   // nothing named, or no device: silence
        }

        PlaySoundParams lParams;
        if (IsClipDefPath(InPath))
        {
            auto lFound = m_Defs.find(InPath);
            if (lFound == m_Defs.end())
            {
                lFound = m_Defs.emplace(InPath, LoadAsset<AudioClipDef>(m_Resources, m_Paths, InPath)).first;
            }

            const AudioClipDef* lDef = lFound->second.Get();
            const Int32 lIndex = lDef != nullptr ? m_Audio.SelectClip(*lDef, OpaaxStringID(InPath), lParams) : -1;
            if (lIndex < 0)
            {
                OPAAX_LOG(LogUIAudio, Warn, "UI sound '{}' is not a clip definition with clips: nothing plays", InPath.CStr());
                return;
            }

            lParams.Volume *= InVolume;
            lParams.Bus      = UI_BUS;
            lParams.bSpatial = false;   // the UI is heard as it is, wherever the listener is
            m_Audio.Play(ResolveClip(lDef->Clips[lIndex].Clip.Path), lParams);
            return;
        }

        lParams.Volume = InVolume;
        lParams.Bus    = UI_BUS;
        m_Audio.Play(ResolveClip(InPath), lParams);
    }

    void UIAudioPlayer::Clear()
    {
        m_Clips.clear();
        m_Defs.clear();
    }

    const TAssetHandle<AudioClip>& UIAudioPlayer::ResolveClip(const OpaaxString& InPath)
    {
        auto lFound = m_Clips.find(InPath);
        if (lFound == m_Clips.end())
        {
            // Loaded once, the first time it plays; kept for the next.
            lFound = m_Clips.emplace(InPath, LoadAsset<AudioClip>(m_Resources, m_Paths, InPath)).first;
        }
        return lFound->second;
    }
}
