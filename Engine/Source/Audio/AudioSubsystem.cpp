#include "Audio/AudioSubsystem.h"

#include "Core/Algo/Algo.h"

#include "Application/Services/IPaths.h"
#include "Audio/AudioClipResource.h"
#include "Audio/AudioGlobalBank.h"
#include "Audio/AudioManager.h"
#include "Audio/Components/AudioBanksComponent.h"
#include "Audio/Components/AudioListenerComponent.h"
#include "Audio/Components/AudioSourceComponent.h"
#include "Core/Events/EventBus.h"
#include "Core/String/OpaaxPathString.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Resources/Asset/AssetFile.h"
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

        /** A sound file's settings from a source that plays one. */
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
            lParams.Attenuation = InSource.Attenuation;
            return lParams;
        }

        Vector2F WorldPositionOf(World& InWorld, const EntityID InEntity)
        {
            return EntityHierarchy::WorldTransform(Entity(InEntity, &InWorld)).Position;
        }

        /** A clip definition is a data asset; anything else is a sound file. */
        bool IsClipDefPath(const OpaaxString& InPath)
        {
            return PathString::Extension(OpaaxStringView(InPath.CStr(), InPath.GetLength())) == OpaaxStringView(AssetFile::ASSET_EXTENSION);
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

        HoldBanks();

        World& lWorld = m_Context->OwningWorld;
        UpdateListener(lWorld, *lAudio);
        UpdateSources(lWorld, *lAudio);

        // Forget the sounds that ended.
        Algo::RemoveIf(m_OneShots, [lAudio](const SoundHandle InSound) { return !lAudio->IsPlaying(InSound); });
        Algo::RemoveIf(m_SourceSounds, [lAudio](const auto& InSource) { return !lAudio->IsPlaying(InSource.second.Sound); });
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

            for (const auto& [lEntity, lSource] : m_SourceSounds)
            {
                lAudio->Stop(lSource.Sound);
            }
        }

        m_OneShots.clear();
        m_SourceSounds.clear();
        m_SeenSources.clear();
        m_Clips.clear();
        m_Defs.clear();
        m_Banks.clear();
    }

    // =========================================================================
    // Sounds
    // =========================================================================
    SoundHandle AudioSubsystem::PlaySound(const OpaaxString& InPath, const PlaySoundParams& InParams)
    {
        const SoundHandle lSound = Start(InPath, InParams);
        if (lSound.IsValid())
        {
            m_OneShots.push_back(lSound);
        }
        return lSound;
    }

    bool AudioSubsystem::PlaySource(const EntityID InEntity)
    {
        World& lWorld = m_Context->OwningWorld;
        if (GetAudio() == nullptr || !lWorld.IsValid(InEntity))
        {
            return false;
        }

        const AudioSourceComponent* lSource = lWorld.GetRegistry().try_get<AudioSourceComponent>(InEntity);
        if (lSource == nullptr || (lSource->Sound.IsEmpty() && lSource->Clip.IsEmpty()))
        {
            return false;
        }

        // One sound per source: playing it again restarts it.
        StopSource(InEntity);

        // Played by hand: it does not also auto-play later.
        m_SeenSources.insert(EntityKey(InEntity));

        const Vector2F lPosition = WorldPositionOf(lWorld, InEntity);
        bool           bSpatial  = false;
        SoundHandle    lSound;

        if (!lSource->Sound.IsEmpty())
        {
            // The definition decides; the source scales its volume and pitch and places it.
            PlaySoundParams lParams;
            lParams.Volume   = lSource->Volume;
            lParams.Pitch    = lSource->Pitch;
            lParams.Position = lPosition;
            lSound = Start(lSource->Sound.Path, lParams, &bSpatial);
        }
        else
        {
            bSpatial = lSource->bSpatial;
            lSound   = Start(lSource->Clip.Path, ParamsOf(*lSource, lPosition));
        }

        if (!lSound.IsValid())
        {
            return false;
        }

        m_SourceSounds[EntityKey(InEntity)] = SourceSound{ lSound, bSpatial };
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
            lAudio->Stop(lFound->second.Sound);
        }
        m_SourceSounds.erase(lFound);
    }

    bool AudioSubsystem::IsSourcePlaying(const EntityID InEntity) const
    {
        const auto lFound = m_SourceSounds.find(EntityKey(InEntity));
        const AudioManager* lAudio = GetAudio();
        return lFound != m_SourceSounds.end() && lAudio != nullptr && lAudio->IsPlaying(lFound->second.Sound);
    }

    void AudioSubsystem::Stop(const SoundHandle InSound, const float InFadeOutSeconds)
    {
        if (AudioManager* lAudio = GetAudio())
        {
            lAudio->Stop(InSound, InFadeOutSeconds);
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
        for (const auto& [lEntity, lSource] : m_SourceSounds)
        {
            if (lAudio->IsPlaying(lSource.Sound)) { ++lCount; }
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
    SoundHandle AudioSubsystem::Start(const OpaaxString& InPath, const PlaySoundParams& InParams, bool* OutSpatial)
    {
        AudioManager* lAudio = GetAudio();
        if (lAudio == nullptr || InPath.IsEmpty())
        {
            return SoundHandle{};
        }

        // A sound played before the first Update (a behaviour's OnStart) finds the level's banks.
        HoldBanks();

        if (!IsClipDefPath(InPath))
        {
            if (OutSpatial != nullptr) { *OutSpatial = InParams.bSpatial; }
            return lAudio->Play(ResolveClip(InPath), InParams);
        }

        const AudioClipDef* lDef = ResolveDef(InPath);
        if (lDef == nullptr)
        {
            return SoundHandle{};
        }

        PlaySoundParams lParams;
        const Int32 lIndex = lAudio->SelectClip(*lDef, OpaaxStringID(InPath), lParams);
        if (lIndex < 0)
        {
            OPAAX_LOG(LogAudio, Warn, "Audio clip definition '{}' has no clips: nothing plays", InPath.CStr());
            return SoundHandle{};
        }

        lParams.Volume        *= InParams.Volume;
        lParams.Pitch         *= InParams.Pitch;
        lParams.FadeInSeconds  = InParams.FadeInSeconds;
        lParams.Position       = InParams.Position;
        lParams.bSpatial       = lParams.bSpatial || InParams.bSpatial;

        if (OutSpatial != nullptr) { *OutSpatial = lParams.bSpatial; }
        return lAudio->Play(ResolveClip(lDef->Clips[lIndex].Clip.Path), lParams);
    }

    const ResourceRef<AudioClipResource>& AudioSubsystem::ResolveClip(const OpaaxString& InPath)
    {
        static const ResourceRef<AudioClipResource> s_None;
        if (InPath.IsEmpty())
        {
            return s_None;
        }

        auto lFound = m_Clips.find(InPath);
        if (lFound != m_Clips.end())
        {
            return lFound->second;
        }

        // A bank's (maybe still loading), or loaded already (another world): it plays as soon as it can.
        // Else it loads without stopping the game.
        if (const ResourceRef<AudioClipResource>* lBanked = FindBankClip(InPath))
        {
            return m_Clips.emplace(InPath, *lBanked).first->second;
        }

        const OpaaxString lAbsolute = m_Context->Paths.AssetToAbsolute(InPath);
        ResourceRef<AudioClipResource> lClip = m_Context->Resources.Find<AudioClipResource>(lAbsolute.CStr());
        if (!lClip.IsValid())
        {
            OPAAX_LOG(LogAudio, Warn, "Audio clip '{}' was not loaded when played: it plays once loaded. List it in "
                      "an audio bank to have it ready", InPath.CStr());
            lClip = m_Context->Resources.LoadAsync<AudioClipResource>(lAbsolute.CStr());
        }

        return m_Clips.emplace(InPath, Move(lClip)).first->second;
    }

    const AudioClipDef* AudioSubsystem::ResolveDef(const OpaaxString& InPath)
    {
        auto lFound = m_Defs.find(InPath);
        if (lFound == m_Defs.end())
        {
            TAssetRef<AudioClipDef> lRef;
            lRef.Path = InPath;
            lFound = m_Defs.emplace(InPath, LoadAsset(m_Context->Resources, m_Context->Paths, lRef)).first;
        }

        return lFound->second.Get();
    }

    void AudioSubsystem::HoldBanks()
    {
        TDynArray<TPair<Uint32, const AudioBanksComponent*>> lNew;
        m_Context->OwningWorld.Each<AudioBanksComponent>([this, &lNew](const EntityID InEntity, const AudioBanksComponent& InBanks)
        {
            if (!m_Banks.contains(EntityKey(InEntity)))
            {
                lNew.emplace_back(EntityKey(InEntity), &InBanks);
            }
        });

        // Held after the view: holding loads files.
        for (const auto& [lKey, lComponent] : lNew)
        {
            AudioBankHold& lHold = m_Banks[lKey];
            for (const TAssetRef<AudioBank>& lBank : lComponent->Banks)
            {
                if (!lBank.IsEmpty())
                {
                    lHold.Hold(lBank.Path, m_Context->Resources, m_Context->Paths);
                }
            }
        }
    }

    const ResourceRef<AudioClipResource>* AudioSubsystem::FindBankClip(const OpaaxString& InPath) const
    {
        for (const auto& [lEntity, lHold] : m_Banks)
        {
            if (const ResourceRef<AudioClipResource>* lClip = lHold.FindClip(InPath))
            {
                return lClip;
            }
        }

        GameInstance* lGame = m_Context->Game;
        const AudioGlobalBank* lGlobal = lGame != nullptr ? lGame->GetSubsystems().GetSubsystem<AudioGlobalBank>() : nullptr;
        return lGlobal != nullptr ? lGlobal->GetHold().FindClip(InPath) : nullptr;
    }

    void AudioSubsystem::OnEntityDestroying(const EntityID InEntity)
    {
        m_SeenSources.erase(EntityKey(InEntity));
        m_Banks.erase(EntityKey(InEntity));

        const auto lFound = m_SourceSounds.find(EntityKey(InEntity));
        if (lFound == m_SourceSounds.end())
        {
            return;
        }

        // A source that outlives its entity plays on as one of the world's sounds.
        const AudioSourceComponent* lSource = m_Context->OwningWorld.GetRegistry().try_get<AudioSourceComponent>(InEntity);
        if (lSource != nullptr && !lSource->bStopOnDestroy)
        {
            m_OneShots.push_back(lFound->second.Sound);
            m_SourceSounds.erase(lFound);
            return;
        }

        StopSource(InEntity);
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

        for (const auto& [lEntity, lSource] : m_SourceSounds)
        {
            lAudio->SetPaused(lSource.Sound, InEvent.bPaused);
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

        if (lPosition != InAudio.GetListenerPosition())
        {
            InAudio.SetListenerPosition(lPosition);
        }
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

            // A spatial source follows its entity (the AudioManager sends only a move).
            const auto lSound = m_SourceSounds.find(lKey);
            if (lSound != m_SourceSounds.end() && lSound->second.bSpatial)
            {
                InAudio.SetPosition(lSound->second.Sound, WorldPositionOf(InWorld, InEntity));
            }
        });

        for (const EntityID lEntity : lToStart)
        {
            PlaySource(lEntity);
        }
    }
}
