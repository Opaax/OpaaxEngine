#pragma once

#include "Audio/AudioTypes.h"
#include "Core/Events/DelegateHandle.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
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
    // AudioSubsystem — a Play world's sounds. Starts its entities' AudioSourceComponents, keeps a
    //   spatial source on its entity and the listener on the AudioListenerComponent (or the
    //   camera), stops an entity's source when the entity is destroyed, pauses the world's sounds
    //   while the world is paused, and stops them when it ends (a level change cuts them).
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
         * Plays a clip, not tied to an entity: it plays to its end (or until the world ends).
         * @param InClipPath Asset-relative ("Audio/Coin.wav")
         * @return Invalid when the clip cannot be loaded or no audio is running
         */
        SoundHandle PlaySound(const OpaaxString& InClipPath, const PlaySoundParams& InParams = {});

        /**
         * Plays InEntity's AudioSourceComponent from the start (stopping it first if it plays).
         * @return False without a source, a clip, or audio
         */
        bool PlaySource(EntityID InEntity);

        void StopSource(EntityID InEntity);
        bool IsSourcePlaying(EntityID InEntity) const;

        void Stop(SoundHandle InSound);

        /** Sounds this world started that still play. */
        Uint64 GetPlayingCount() const;

        /** The engine's audio, or null (a bare world). */
        AudioManager* GetAudio() const noexcept;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Loaded once and kept for the world's life. Null when it cannot be loaded. */
        const AudioClipResource* ResolveClip(const OpaaxString& InPath);

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

        /** The sound of each entity's source, keyed by entity. */
        TUnorderedMap<Uint32, SoundHandle> m_SourceSounds;

        /** Entities whose source was already considered for bPlayOnStart. */
        TUnorderedSet<Uint32> m_SeenSources;

        /** PlaySound's sounds. */
        TDynArray<SoundHandle> m_OneShots;

        DelegateHandle m_EntityDestroyingHandle;
    };
}
