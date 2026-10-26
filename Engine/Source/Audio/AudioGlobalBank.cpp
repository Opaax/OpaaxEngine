#include "Audio/AudioGlobalBank.h"

#include "Audio/AudioTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstanceContext.h"

namespace Opaax
{
    AudioGlobalBank::AudioGlobalBank(GameInstanceContext& InContext)
        : m_Context(&InContext)
    {
    }

    bool AudioGlobalBank::Startup()
    {
        const TAssetRef<AudioBank>& lBank = m_Context->Config.Audio.GlobalBank;
        if (!lBank.IsEmpty() && m_Hold.Hold(lBank.Path, m_Context->Resources, m_Context->Paths))
        {
            OPAAX_LOG(LogAudio, Info, "Global audio bank '{}': {} sound(s) held for the game", lBank.Path.CStr(),
                      m_Hold.GetClipCount());
        }
        return true;
    }

    void AudioGlobalBank::Shutdown()
    {
        m_Hold.Release();
    }
}
