#pragma once

#include "Audio/AudioBankHold.h"
#include "Audio/AudioClipDef.h"
#include "Audio/AudioTypes.h"
#include "Core/Events/DelegateHandle.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Resources/DataAsset/DataAssetHandle.h"
#include "Resources/ResourceRef.hpp"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class AudioManager;
    class World;
    struct WorldPauseChanged;
    struct AudioClipResource;
    struct WorldContext;

    // =============================================================================
    // AudioSubsystem — a Play world's sounds. Plays sound files and clip definitions by path,
    //   starts its entities' AudioSourceComponents, keeps a spatial source on its entity and the
    //   listener on the AudioListenerComponent (or the camera), stops an entity's source when the
    //   entity is destroyed, pauses the world's sounds while the world is paused, and stops them when
    //   it ends (a level change cuts them).
    //
    //   Holds the banks of its AudioBanks components while their entities live. A clip is expected
    //   loaded (a bank, or the global bank): one that is not is logged, loaded in the background,
    //   and plays once loaded; the world then keeps it until it ends.
    // =============================================================================
    class AudioSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(AudioSubsystem)

        /** Play worlds only. */
        static bool ShouldCreate(const World& InWorld);

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        explicit AudioSubsystem(WorldContext& InContext);
        ~AudioSubsystem() override;

        AudioSubsystem(const AudioSubsystem&)            = delete;
        AudioSubsystem& operator=(const AudioSubsystem&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Sounds
        // =============================================================================
    public:
        /**
         * Plays a sound file or a clip definition, not tied to an entity: it plays to its end (or
         * until the world ends). For a clip definition, its settings win, and InParams adds to them:
         * its volume and pitch multiply, its position, fade-in, and bSpatial (on) are used.
         * @param InPath Asset-relative: "Audio/Coin.wav", or "Audio/Footsteps.opaaxdata"
         * @return Invalid when nothing can play (a missing file, a definition with no clips, no audio)
         */
        SoundHandle PlaySound(const OpaaxString& InPath, const PlaySoundParams& InParams = {});

        /**
         * Plays InEntity's AudioSourceComponent from the start (stopping it first if it plays).
         * @return False without a source, a sound, or audio
         */
        bool PlaySource(EntityID InEntity);

        void StopSource(EntityID InEntity);
        bool IsSourcePlaying(EntityID InEntity) const;

        void Stop(SoundHandle InSound, float InFadeOutSeconds = 0.f);

        /** Sounds this world started that still play. */
        Uint64 GetPlayingCount() const;

        /** The engine's audio, or null (a bare world). */
        AudioManager* GetAudio() const noexcept;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** A source's sound, and whether it follows its entity. */
        struct SourceSound
        {
            SoundHandle Sound;
            bool        bSpatial = false;
        };

        /** Starts InPath (a file or a definition) with InParams. OutSpatial: whether it plays in space. */
        SoundHandle Start(const OpaaxString& InPath, const PlaySoundParams& InParams, bool* OutSpatial = nullptr);

        /** The clip at InPath, loaded or loading; kept for the world's life. Empty for an empty path. */
        const ResourceRef<AudioClipResource>& ResolveClip(const OpaaxString& InPath);

        /** The clip definition at InPath, kept for the world's life. Null when it does not load (logged). */
        const AudioClipDef* ResolveDef(const OpaaxString& InPath);

        /** Holds the banks of AudioBanks components not seen yet. */
        void HoldBanks();

        /** A clip held by one of the world's banks or the global bank, or null. */
        const ResourceRef<AudioClipResource>* FindBankClip(const OpaaxString& InPath) const;

        void OnEntityDestroying(EntityID InEntity);

        /** Pauses or resumes every sound of the world with it. */
        void OnWorldPauseChanged(const WorldPauseChanged& InEvent);
        void UpdateListener(World& InWorld, AudioManager& InAudio);
        void UpdateSources(World& InWorld, AudioManager& InAudio);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        TUnorderedMap<OpaaxString, ResourceRef<AudioClipResource>> m_Clips;
        TUnorderedMap<OpaaxString, TDataAssetHandle<AudioClipDef>> m_Defs;

        /** The banks of each AudioBanks component, keyed by entity. */
        TUnorderedMap<Uint32, AudioBankHold> m_Banks;

        /** The sound of each entity's source, keyed by entity. */
        TUnorderedMap<Uint32, SourceSound> m_SourceSounds;

        /** Entities whose source was already considered for bPlayOnStart. */
        TUnorderedSet<Uint32> m_SeenSources;

        /** PlaySound's sounds, and sources left playing by their destroyed entity. */
        TDynArray<SoundHandle> m_OneShots;

        DelegateHandle m_EntityDestroyingHandle;
    };
}
