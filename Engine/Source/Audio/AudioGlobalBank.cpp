#include "Audio/AudioGlobalBank.h"

#include "Audio/AudioTypes.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/GameInstance/GameInstanceContext.h"

namespace Opaax
{
    bool AudioGlobalBank::Startup()
    {
        const TAssetRef<AudioBank>& lBank = GetContext().Config.Audio.GlobalBank;
        if (!lBank.IsEmpty() && m_Hold.Hold(lBank.Path, GetContext().Resources, GetContext().Paths))
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
