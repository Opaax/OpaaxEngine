#include "Renderer/Camera/CameraShakeSubsystem.h"

#include "Core/Algo/Algo.h"
#include "Core/Maths/Maths.h"
#include "Renderer/Camera/CameraDirector.h"
#include "Renderer/Camera/CameraShakeSourceComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** InPlaying's stop as seconds after its start, the way CameraShake takes it. */
        TOptional<double> StoppedAfter(const CameraShakePlaying& InPlaying)
        {
            return InPlaying.StoppedAt.has_value() ? TOptional<double>(*InPlaying.StoppedAt - InPlaying.Start)
                                                   : TOptional<double>();
        }
    }

    // =========================================================================
    // CameraShakeModifier
    // =========================================================================
    void CameraShakeModifier::Modify(const CameraModifierContext& InContext, CameraView& InOutView) const
    {
        const CameraShakeSubsystem* lShakes = Shakes.Get();
        if (lShakes == nullptr)
        {
            return;
        }

        const CameraShakeOffset lOffset = lShakes->Evaluate(InContext.GameTime, InOutView.Position);

        // Along the view's axes: a rolled view shakes as an upright one does.
        const float lRad = Maths::DegreesToRadians(InOutView.Rotation);
        const float lCos = Maths::Cos(lRad);
        const float lSin = Maths::Sin(lRad);
        InOutView.Position += Vector2F(lCos * lOffset.Position.x - lSin * lOffset.Position.y,
                                       lSin * lOffset.Position.x + lCos * lOffset.Position.y);
        InOutView.Rotation += lOffset.Rotation;
    }

    // =========================================================================
    // CameraShakeSubsystem
    // =========================================================================
    bool CameraShakeSubsystem::ShouldCreate(const World& InWorld) const
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool CameraShakeSubsystem::Startup()
    {
        // Every subsystem is made before any starts: the director is there if the world has one.
        CameraDirector* lDirector = GetWorld().GetSubsystems().GetSubsystem<CameraDirector>();
        if (lDirector == nullptr)
        {
            OPAAX_LOG(LogCameraShake, Warn, "This world has no camera director: its shakes are not drawn");
            return true;
        }

        m_Modifier         = NewObject<CameraShakeModifier>();
        m_Modifier->Shakes = this;
        lDirector->AddModifier(TStrongObjectPtr<CameraModifier>(m_Modifier));
        return true;
    }

    void CameraShakeSubsystem::Shutdown()
    {
        CameraDirector* lDirector = GetWorld().GetSubsystems().GetSubsystem<CameraDirector>();
        if (m_Modifier && lDirector != nullptr)
        {
            lDirector->RemoveModifier(*m_Modifier);
        }

        m_Modifier.Reset();
        m_Playing.clear();
    }

    void CameraShakeSubsystem::FixedUpdate(double)
    {
        World& lWorld = GetWorld();

        // Sources seen for the first time: played if they play on start (a trauma source always plays).
        // Collected first: playing loads the shake, which must not happen inside the view.
        TDynArray<EntityID> lToPlay;
        lWorld.Each<CameraShakeSourceComponent>([&](const EntityID InEntity, const CameraShakeSourceComponent& InSource)
        {
            if (m_SeenSources.insert(static_cast<Uint32>(InEntity)).second && (InSource.bPlayOnStart || InSource.bUseTrauma))
            {
                lToPlay.push_back(InEntity);
            }
        });

        for (const EntityID lSource : lToPlay)
        {
            PlaySource(lSource);
        }

        // A source's shake is attenuated from where its entity was at this step; stopped once it is gone.
        const double lNow = lWorld.GetSteppedTime();
        for (CameraShakePlaying& lPlaying : m_Playing)
        {
            if (lPlaying.Source == ENTITY_NONE)
            {
                continue;
            }

            Entity lEntity(lPlaying.Source, &lWorld);
            const CameraShakeSourceComponent* lSource = lEntity.IsValid() ? lEntity.TryGet<CameraShakeSourceComponent>()
                                                                          : nullptr;
            if (lSource != nullptr)
            {
                lPlaying.SourceAt    = EntityHierarchy::WorldTransform(lEntity).Position;
                lPlaying.InnerRadius = lSource->InnerRadius;
                lPlaying.OuterRadius = lSource->OuterRadius;
            }
            else if (!lPlaying.StoppedAt.has_value())
            {
                lPlaying.StoppedAt = lNow;
            }
        }

        // Let go once over for every frame still to come: the earliest a frame shows is a step behind.
        const double lSettled = lNow - lWorld.GetLastStepDelta();
        Algo::RemoveIf(m_Playing, [lSettled](const CameraShakePlaying& InPlaying)
        {
            const CameraShake* lShake = InPlaying.Get();
            return lShake == nullptr || lSettled - InPlaying.Start >= lShake->EndTime(StoppedAfter(InPlaying));
        });
    }

    // =========================================================================
    // Playing
    // =========================================================================
    Uint32 CameraShakeSubsystem::Start(CameraShakePlaying InPlaying)
    {
        if (InPlaying.Get() == nullptr)
        {
            OPAAX_LOG(LogCameraShake, Warn, "Camera shake '{}' did not load: not played", InPlaying.Asset.GetPath().CStr());
            return 0;
        }

        InPlaying.Id    = m_NextId++;
        InPlaying.Start = GetWorld().GetSteppedTime();
        m_Playing.push_back(Move(InPlaying));
        return m_Playing.back().Id;
    }

    Uint32 CameraShakeSubsystem::Play(const TAssetHandle<CameraShake>& InShake, const float InScale, const Uint32 InSeed)
    {
        CameraShakePlaying lPlaying;
        lPlaying.Asset = InShake;
        lPlaying.Scale = InScale;
        lPlaying.Seed  = InSeed;
        return Start(Move(lPlaying));
    }

    Uint32 CameraShakeSubsystem::Play(TStrongObjectPtr<CameraShake> InShake, const float InScale, const Uint32 InSeed)
    {
        CameraShakePlaying lPlaying;
        lPlaying.Owned = Move(InShake);
        lPlaying.Scale = InScale;
        lPlaying.Seed  = InSeed;
        return Start(Move(lPlaying));
    }

    Uint32 CameraShakeSubsystem::Play(const TAssetRef<CameraShake>& InShake, const float InScale, const Uint32 InSeed)
    {
        if (InShake.IsEmpty())
        {
            return 0;
        }

        return Play(LoadAsset(GetContext().Resources, GetContext().Paths, InShake), InScale, InSeed);
    }

    void CameraShakeSubsystem::Stop(const Uint32 InId)
    {
        for (CameraShakePlaying& lPlaying : m_Playing)
        {
            if (lPlaying.Id == InId && !lPlaying.StoppedAt.has_value())
            {
                lPlaying.StoppedAt = GetWorld().GetSteppedTime();
            }
        }
    }

    void CameraShakeSubsystem::StopAll()
    {
        for (CameraShakePlaying& lPlaying : m_Playing)
        {
            if (!lPlaying.StoppedAt.has_value())
            {
                lPlaying.StoppedAt = GetWorld().GetSteppedTime();
            }
        }
    }

    Uint32 CameraShakeSubsystem::PlaySource(const EntityID InSource)
    {
        Entity lEntity(InSource, &GetWorld());
        const CameraShakeSourceComponent* lSource = lEntity.IsValid() ? lEntity.TryGet<CameraShakeSourceComponent>()
                                                                      : nullptr;
        if (lSource == nullptr || lSource->Shake.IsEmpty())
        {
            return 0;
        }

        // Played again: the one under way fades out, and its trauma carries over.
        CameraShakePlaying lPlaying;
        if (CameraShakePlaying* lUnderWay = FindSource(InSource))
        {
            lPlaying.Trauma = lUnderWay->Trauma;
            Stop(lUnderWay->Id);
        }

        lPlaying.Asset        = LoadAsset(GetContext().Resources, GetContext().Paths, lSource->Shake);
        lPlaying.Scale        = lSource->Scale;
        lPlaying.Seed         = lSource->Seed;
        lPlaying.Source       = InSource;
        lPlaying.SourceAt     = EntityHierarchy::WorldTransform(lEntity).Position;
        lPlaying.InnerRadius  = lSource->InnerRadius;
        lPlaying.OuterRadius  = lSource->OuterRadius;
        lPlaying.bTrauma      = lSource->bUseTrauma;
        lPlaying.Trauma.Decay = lSource->TraumaDecay;
        m_SeenSources.insert(static_cast<Uint32>(InSource));
        return Start(Move(lPlaying));
    }

    void CameraShakeSubsystem::AddTrauma(const EntityID InSource, const float InAmount)
    {
        Entity lEntity(InSource, &GetWorld());
        const CameraShakeSourceComponent* lSource = lEntity.IsValid() ? lEntity.TryGet<CameraShakeSourceComponent>()
                                                                      : nullptr;
        if (lSource == nullptr || !lSource->bUseTrauma)
        {
            OPAAX_LOG(LogCameraShake, Warn, "Trauma added to an entity without a trauma source (a CameraShakeSource "
                      "with bUseTrauma): ignored");
            return;
        }

        // Not seen yet (spawned this step): its shake starts now.
        CameraShakePlaying* lPlaying = FindSource(InSource);
        if (lPlaying == nullptr && PlaySource(InSource) != 0)
        {
            lPlaying = FindSource(InSource);
        }

        if (lPlaying == nullptr)
        {
            return;
        }

        lPlaying->Trauma.Add(InAmount, GetWorld().GetSteppedTime());
    }

    float CameraShakeSubsystem::GetTrauma(const EntityID InSource) const
    {
        const CameraShakePlaying* lPlaying = FindSource(InSource);
        return lPlaying != nullptr && lPlaying->bTrauma ? lPlaying->Trauma.At(GetWorld().GetSteppedTime()) : 0.f;
    }

    CameraShakeOffset CameraShakeSubsystem::Evaluate(const double InGameTime, const Vector2F InCentre) const
    {
        CameraShakeOffset lTotal;
        for (const CameraShakePlaying& lPlaying : m_Playing)
        {
            const CameraShake* lShake = lPlaying.Get();
            if (lShake == nullptr)
            {
                continue;
            }

            float lScale = lPlaying.Scale;
            if (lPlaying.Source != ENTITY_NONE)
            {
                lScale *= CameraShakeAttenuation(glm::length(lPlaying.SourceAt - InCentre), lPlaying.InnerRadius,
                                                 lPlaying.OuterRadius);
            }

            if (lPlaying.bTrauma)
            {
                const float lTrauma = lPlaying.Trauma.At(InGameTime);
                lScale *= lTrauma * lTrauma;
            }

            if (lScale <= 0.f)
            {
                continue;
            }

            const CameraShakeOffset lOffset = lShake->Sample(lPlaying.Seed, InGameTime - lPlaying.Start,
                                                             StoppedAfter(lPlaying));
            lTotal.Position += lOffset.Position * lScale;
            lTotal.Rotation += lOffset.Rotation * lScale;
        }

        return lTotal;
    }

    CameraShakePlaying* CameraShakeSubsystem::FindSource(const EntityID InSource)
    {
        for (CameraShakePlaying& lPlaying : m_Playing)
        {
            if (lPlaying.Source == InSource && !lPlaying.StoppedAt.has_value())
            {
                return &lPlaying;
            }
        }

        return nullptr;
    }

    const CameraShakePlaying* CameraShakeSubsystem::FindSource(const EntityID InSource) const
    {
        return const_cast<CameraShakeSubsystem*>(this)->FindSource(InSource);
    }
}
