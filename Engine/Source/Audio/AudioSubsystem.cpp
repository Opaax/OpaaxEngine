#include "Audio/AudioSubsystem.h"

#include <algorithm>

#include "Application/Services/IPaths.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioManager.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Core/Events/EventBus.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Resources/ResourceManager.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldEvents.h"

namespace Opaax
{
    namespace
    {
        Uint32 EntityKey(const EntityID InEntity) noexcept
        {
            return static_cast<Uint32>(InEntity);
        }

        PlaySoundParams ParamsOf(const AudioSourceComponent& InSource, const Vector2F InPosition)
        {
            PlaySoundParams lParams;
            lParams.Bus         = InSource.Bus;
            lParams.Volume      = InSource.Volume;
            lParams.Pitch       = InSource.Pitch;
            lParams.bLoop       = InSource.bLoop;
            lParams.bSpatial    = InSource.bSpatial;
            lParams.Position    = InPosition;
            lParams.MinDistance = InSource.MinDistance;
            lParams.MaxDistance = InSource.MaxDistance;
            return lParams;
        }

        Vector2F WorldPositionOf(World& InWorld, const EntityID InEntity)
        {
            return EntityHierarchy::WorldTransform(Entity(InEntity, &InWorld)).Position;
        }
    }

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    bool AudioSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    AudioSubsystem::AudioSubsystem(WorldContext& InContext)
        : m_Context(&InContext)
    {
    }

    AudioSubsystem::~AudioSubsystem() = default;

    // =========================================================================
    // ISubsystem
    // =========================================================================
    bool AudioSubsystem::Startup()
    {
        m_EntityDestroyingHandle = m_Context->OwningWorld.OnEntityDestroying()
                                       .AddMember(this, &AudioSubsystem::OnEntityDestroying);

        m_Context->Events.GetEventBus().Subscribe<WorldPauseChanged>(this, &AudioSubsystem::OnWorldPauseChanged);
        return true;
    }

    void AudioSubsystem::Update(const double /*InDeltaTime*/)
    {
        AudioManager* lAudio = GetAudio();
        if (lAudio == nullptr || !lAudio->IsStarted())
        {
            return;
        }

        World& lWorld = m_Context->OwningWorld;
        UpdateListener(lWorld, *lAudio);
        UpdateSources(lWorld, *lAudio);

        // Forget the sounds that ended.
        m_OneShots.erase(std::remove_if(m_OneShots.begin(), m_OneShots.end(),
                                        [lAudio](const SoundHandle InSound) { return !lAudio->IsPlaying(InSound); }),
                         m_OneShots.end());

        for (auto lIt = m_SourceSounds.begin(); lIt != m_SourceSounds.end();)
        {
            lIt = lAudio->IsPlaying(lIt->second) ? std::next(lIt) : m_SourceSounds.erase(lIt);
        }
    }

    void AudioSubsystem::Shutdown()
    {
        if (m_EntityDestroyingHandle.IsValid())
        {
            m_Context->OwningWorld.OnEntityDestroying().Remove(m_EntityDestroyingHandle);
            m_EntityDestroyingHandle = DelegateHandle();
        }

        m_Context->Events.GetEventBus().UnsubscribeAll(this);

        // The world's sounds end with it.
        if (AudioManager* lAudio = GetAudio())
        {
            for (const SoundHandle lSound : m_OneShots)
            {
                lAudio->Stop(lSound);
            }

            for (const auto& [lEntity, lSound] : m_SourceSounds)
            {
                lAudio->Stop(lSound);
            }
        }

        m_OneShots.clear();
        m_SourceSounds.clear();
        m_SeenSources.clear();
        m_Clips.clear();
    }

    // =========================================================================
    // Sounds
    // =========================================================================
    SoundHandle AudioSubsystem::PlaySound(const OpaaxString& InClipPath, const PlaySoundParams& InParams)
    {
        AudioManager* lAudio = GetAudio();
        if (lAudio == nullptr)
        {
            return SoundHandle{};
        }

        const AudioClipResource* lClip = ResolveClip(InClipPath);
        if (lClip == nullptr)
        {
            return SoundHandle{};
        }

        const SoundHandle lSound = lAudio->Play(*lClip, InParams);
        if (lSound.IsValid())
        {
            m_OneShots.push_back(lSound);
        }
        return lSound;
    }

    bool AudioSubsystem::PlaySource(const EntityID InEntity)
    {
        AudioManager* lAudio = GetAudio();
        World&        lWorld = m_Context->OwningWorld;
        if (lAudio == nullptr || !lWorld.IsValid(InEntity))
        {
            return false;
        }

        const AudioSourceComponent* lSource = lWorld.GetRegistry().try_get<AudioSourceComponent>(InEntity);
        if (lSource == nullptr || lSource->Clip.IsEmpty())
        {
            return false;
        }

        const AudioClipResource* lClip = ResolveClip(lSource->Clip.Path);
        if (lClip == nullptr)
        {
            return false;
        }

        // One sound per source: playing it again restarts it.
        StopSource(InEntity);

        // Played by hand: it does not also auto-play later.
        m_SeenSources.insert(EntityKey(InEntity));

        const SoundHandle lSound = lAudio->Play(*lClip, ParamsOf(*lSource, WorldPositionOf(lWorld, InEntity)));
        if (!lSound.IsValid())
        {
            return false;
        }

        m_SourceSounds[EntityKey(InEntity)] = lSound;
        return true;
    }

    void AudioSubsystem::StopSource(const EntityID InEntity)
    {
        const auto lFound = m_SourceSounds.find(EntityKey(InEntity));
        if (lFound == m_SourceSounds.end())
        {
            return;
        }

        if (AudioManager* lAudio = GetAudio())
        {
            lAudio->Stop(lFound->second);
        }
        m_SourceSounds.erase(lFound);
    }

    bool AudioSubsystem::IsSourcePlaying(const EntityID InEntity) const
    {
        const auto lFound = m_SourceSounds.find(EntityKey(InEntity));
        const AudioManager* lAudio = GetAudio();
        return lFound != m_SourceSounds.end() && lAudio != nullptr && lAudio->IsPlaying(lFound->second);
    }

    void AudioSubsystem::Stop(const SoundHandle InSound)
    {
        if (AudioManager* lAudio = GetAudio())
        {
            lAudio->Stop(InSound);
        }
    }

    Uint64 AudioSubsystem::GetPlayingCount() const
    {
        const AudioManager* lAudio = GetAudio();
        if (lAudio == nullptr)
        {
            return 0;
        }

        Uint64 lCount = 0;
        for (const SoundHandle lSound : m_OneShots)
        {
            if (lAudio->IsPlaying(lSound)) { ++lCount; }
        }
        for (const auto& [lEntity, lSound] : m_SourceSounds)
        {
            if (lAudio->IsPlaying(lSound)) { ++lCount; }
        }
        return lCount;
    }

    AudioManager* AudioSubsystem::GetAudio() const noexcept
    {
        return m_Context->Audio;
    }

    // =========================================================================
    // Internal
    // =========================================================================
    const AudioClipResource* AudioSubsystem::ResolveClip(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty())
        {
            return nullptr;
        }

        auto lFound = m_Clips.find(InPath);
        if (lFound == m_Clips.end())
        {
            const OpaaxString lAbsolute = m_Context->Paths.AssetToAbsolute(InPath);
            lFound = m_Clips.emplace(InPath, m_Context->Resources.Load<AudioClipResource>(lAbsolute.CStr())).first;
        }

        // A clip that failed to load is its silent placeholder.
        const AudioClipResource* lClip = lFound->second.Get();
        return (lClip != nullptr && !lClip->IsSilent()) ? lClip : nullptr;
    }

    void AudioSubsystem::OnEntityDestroying(const EntityID InEntity)
    {
        StopSource(InEntity);
        m_SeenSources.erase(EntityKey(InEntity));
    }

    void AudioSubsystem::OnWorldPauseChanged(const WorldPauseChanged& InEvent)
    {
        AudioManager* lAudio = GetAudio();
        if (lAudio == nullptr)
        {
            return;
        }

        for (const SoundHandle lSound : m_OneShots)
        {
            lAudio->SetPaused(lSound, InEvent.bPaused);
        }

        for (const auto& [lEntity, lSound] : m_SourceSounds)
        {
            lAudio->SetPaused(lSound, InEvent.bPaused);
        }
    }

    void AudioSubsystem::UpdateListener(World& InWorld, AudioManager& InAudio)
    {
        // The first active listener, else the camera's centre.
        Vector2F lPosition = InWorld.GetCameraView().Position;
        bool     bFound    = false;

        InWorld.Each<AudioListenerComponent>([&](const EntityID InEntity, const AudioListenerComponent& InListener)
        {
            if (!bFound && InListener.bActive)
            {
                bFound    = true;
                lPosition = WorldPositionOf(InWorld, InEntity);
            }
        });

        InAudio.SetListenerPosition(lPosition);
    }

    void AudioSubsystem::UpdateSources(World& InWorld, AudioManager& InAudio)
    {
        // Collected first: playing loads clips, which must not happen inside the view.
        TDynArray<EntityID> lToStart;

        InWorld.Each<AudioSourceComponent>([&](const EntityID InEntity, const AudioSourceComponent& InSource)
        {
            const Uint32 lKey = EntityKey(InEntity);

            // Each source is considered once for bPlayOnStart.
            if (m_SeenSources.insert(lKey).second && InSource.bPlayOnStart)
            {
                lToStart.push_back(InEntity);
                return;
            }

            // A spatial source follows its entity.
            const auto lSound = m_SourceSounds.find(lKey);
            if (lSound != m_SourceSounds.end() && InSource.bSpatial)
            {
                InAudio.SetPosition(lSound->second, WorldPositionOf(InWorld, InEntity));
            }
        });

        for (const EntityID lEntity : lToStart)
        {
            PlaySource(lEntity);
        }
    }
}
