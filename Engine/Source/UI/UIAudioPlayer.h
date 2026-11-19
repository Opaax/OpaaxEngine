#pragma once

#include "Audio/AudioClip.h"
#include "Audio/AudioClipDef.h"
#include "Core/OpaaxTypes.h"
#include "Resources/Asset/AssetHandle.h"
#include "UI/UIAudio.h"

namespace Opaax
{
    class AudioManager;
    class IPaths;
    class ResourceManager;

    // =============================================================================
    // UIAudioPlayer — the engine's IUIAudio: plays on the AudioManager, on the "UI" bus, which no world
    //   pauses. Keeps each clip and definition it played loaded, so a hover sound is ready the next time.
    // =============================================================================
    class UIAudioPlayer final : public IUIAudio
    {
    public:
        UIAudioPlayer(AudioManager& InAudio, ResourceManager& InResources, const IPaths& InPaths);

        //~Begin IUIAudio interface
        void PlaySound(const OpaaxString& InPath, float InVolume) override;
        //~End IUIAudio interface

        /** Lets go of the clips and definitions it kept. */
        void Clear();

        /** The sound files it keeps loaded. */
        Uint64 GetHeldClipCount() const noexcept { return m_Clips.size(); }

    private:
        const TAssetHandle<AudioClip>& ResolveClip(const OpaaxString& InPath);

        AudioManager&    m_Audio;
        ResourceManager& m_Resources;
        const IPaths&    m_Paths;

        TUnorderedMap<OpaaxString, TAssetHandle<AudioClip>>    m_Clips;
        TUnorderedMap<OpaaxString, TAssetHandle<AudioClipDef>> m_Defs;
    };
}
