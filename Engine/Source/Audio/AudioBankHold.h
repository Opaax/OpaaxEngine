#pragma once

#include "Audio/AudioBank.h"
#include "Audio/AudioClip.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/Asset/AssetHandle.h"

namespace Opaax
{
    class IPaths;
    class ResourceManager;

    // =============================================================================
    // AudioBankHold — keeps banks' sounds loaded while it holds them: every clip of their clip
    //   definitions, and their sound files. Clips load in the background (a short one is decoded
    //   there too); releasing the hold lets them go, unless something else holds them, and a sound
    //   still playing keeps its own data.
    // =============================================================================
    class AudioBankHold
    {
    public:
        /** Loads the bank at InBankPath (asset-relative) and holds its sounds. False when it does not load (logged). */
        bool Hold(const OpaaxString& InBankPath, ResourceManager& InResources, const IPaths& InPaths);

        /** Holds InBank's sounds, besides what is already held. */
        void Hold(const AudioBank& InBank, ResourceManager& InResources, const IPaths& InPaths);

        void Release();

        /** A held clip by its asset-relative path (loaded or loading), or null. */
        const TAssetHandle<AudioClip>* FindClip(const OpaaxString& InPath) const;

        Uint32 GetClipCount() const noexcept { return static_cast<Uint32>(m_Clips.size()); }

    private:
        void HoldClip(const OpaaxString& InPath, ResourceManager& InResources, const IPaths& InPaths);

        TDynArray<TAssetHandle<AudioBank>>                  m_Banks;
        TDynArray<TAssetHandle<AudioClipDef>>               m_Defs;
        TUnorderedMap<OpaaxString, TAssetHandle<AudioClip>> m_Clips;
    };
}
