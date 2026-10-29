#include "Renderer/Camera/CameraDirector.h"

#include "Core/Algo/Algo.h"
#include "Renderer/Camera/CameraComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** InShot's camera at InAlpha, or where it was last seen once it is gone. */
        CameraView ViewOf(const CameraShot& InShot, World& InWorld, const TOptional<float> InAlpha)
        {
            const Entity lCamera(InShot.Camera, &InWorld);
            return lCamera.IsValid() && lCamera.Has<CameraComponent>() ? CameraManager::ResolveCamera(lCamera, InAlpha)
                                                                      : InShot.LastView;
        }

        /** How far InShot has blended in at InTime, 0 to 1. */
        float WeightOf(const CameraShot& InShot, const double InTime)
        {
            const double lElapsed = InTime - InShot.Start;
            return CameraBlendWeight(InShot.Curve, InShot.Duration > 0.f ? static_cast<float>(lElapsed / InShot.Duration)
                                                                         : 1.f);
        }
    }

    bool CameraDirector::ShouldCreate(const World& InWorld) const
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    void CameraDirector::FixedUpdate(double)
    {
        Direct();
    }

    double CameraDirector::GameTimeAt(const World& InWorld, const TOptional<float> InAlpha) noexcept
    {
        const double lAlpha = InAlpha.has_value() ? static_cast<double>(*InAlpha) : 1.0;
        return InWorld.GetSteppedTime() - (1.0 - lAlpha) * InWorld.GetLastStepDelta();
    }

    // =========================================================================
    // Direct — in a fixed step the stepped time is the start of that step (the world adds the step
    //   after its subsystems ran); between steps, the end of the last one.
    // =========================================================================
    void CameraDirector::Direct()
    {
        // By priority, equals in the order added: a priority changed since counts from this frame.
        Algo::StableSort(m_Modifiers, [](const TStrongObjectPtr<CameraModifier>& InLeft,
                                         const TStrongObjectPtr<CameraModifier>& InRight)
        {
            return InLeft->Priority < InRight->Priority;
        });

        World&                 lWorld = GetWorld();
        const CameraResolution lFound = CameraManager::FindLiveCamera(lWorld);
        m_Count = lFound.Count;
        m_Tied  = lFound.Tied;

        // Where each camera is, so one destroyed mid-blend is held there.
        for (CameraShot& lShot : m_Shots)
        {
            const Entity lCamera(lShot.Camera, &lWorld);
            if (lCamera.IsValid() && lCamera.Has<CameraComponent>())
            {
                lShot.LastView = CameraManager::ResolveCamera(lCamera);
            }
        }

        // A blend over by the start of the last step is drawn whole on every frame from now on (none
        // shows an earlier time): what it blended over is no longer seen.
        const double lNow     = lWorld.GetSteppedTime();
        const double lSettled = lNow - lWorld.GetLastStepDelta();
        for (Uint64 lIndex = m_Shots.size(); lIndex-- > 1;)
        {
            if (m_Shots[lIndex].Start + m_Shots[lIndex].Duration <= lSettled)
            {
                m_Shots.erase(m_Shots.begin(), m_Shots.begin() + static_cast<Int64>(lIndex));
                break;
            }
        }

        if (lFound.Entity == GetLive())
        {
            return;
        }

        if (lFound.Count == 0)
        {
            m_Shots.clear();
            return;
        }

        Entity                 lCamera(lFound.Entity, &lWorld);
        const CameraComponent& lSettings = lCamera.Get<CameraComponent>();

        CameraShot lShot;
        lShot.Camera   = lFound.Entity;
        lShot.LastView = CameraManager::ResolveCamera(lCamera);
        lShot.Start    = lNow;
        lShot.Duration = lSettings.BlendIn;
        lShot.Curve    = lSettings.BlendCurve;

        // The first camera, or a cut: nothing to blend from.
        if (m_Shots.empty() || lSettings.BlendIn <= 0.f || lSettings.BlendCurve == ECameraBlend::Cut)
        {
            lShot.Duration = 0.f;
            lShot.Curve    = ECameraBlend::Cut;
            m_Shots.clear();
            m_Shots.push_back(lShot);
            return;
        }

        if (m_Shots.size() >= MAX_SHOTS)
        {
            m_Shots.erase(m_Shots.begin());
            if (!m_bWarnedShots)
            {
                m_bWarnedShots = true;
                OPAAX_LOG(LogCameraManager, Warn, "World '{}' switches cameras faster than its blends end ({} blends "
                                                  "under way): the oldest is dropped, and the view may jump. Shorten "
                                                  "the BlendIn of the cameras, or switch less often.",
                          lWorld.GetName().CStr(), MAX_SHOTS);
            }
        }
        m_Shots.push_back(lShot);
    }

    CameraResolution CameraDirector::Frame(const TOptional<float> InAlpha) const
    {
        CameraResolution lResolution = Blend(InAlpha);
        lResolution.Unmodified = lResolution.View;

        const CameraModifierContext lContext{ GetWorld(), GameTimeAt(GetWorld(), InAlpha), lResolution.Entity };
        for (const TStrongObjectPtr<CameraModifier>& lModifier : m_Modifiers)
        {
            lModifier->Modify(lContext, lResolution.View);
        }
        return lResolution;
    }

    CameraResolution CameraDirector::Blend(const TOptional<float> InAlpha) const
    {
        World& lWorld = GetWorld();
        if (m_Shots.empty())
        {
            CameraResolution lResolution = CameraManager::FindLiveCamera(lWorld);
            if (lResolution.Count > 0)
            {
                lResolution.View = CameraManager::ResolveCamera(Entity(lResolution.Entity, &lWorld), InAlpha);
            }
            return lResolution;
        }

        CameraResolution lResolution;
        lResolution.Count  = m_Count;
        lResolution.Tied   = m_Tied;
        lResolution.Entity = GetLive();

        // Each shot blended in over the view of the ones before it.
        const double lTime = GameTimeAt(lWorld, InAlpha);
        CameraView   lView = ViewOf(m_Shots.front(), lWorld, InAlpha);
        float        lLast = 1.f;
        for (Uint64 lIndex = 1; lIndex < m_Shots.size(); ++lIndex)
        {
            lLast = WeightOf(m_Shots[lIndex], lTime);
            lView = BlendCameraViews(lView, ViewOf(m_Shots[lIndex], lWorld, InAlpha), lLast);
        }

        lResolution.View        = lView;
        lResolution.Blends      = static_cast<Uint32>(m_Shots.size() - 1);
        lResolution.BlendWeight = lLast;
        if (m_Shots.size() > 1)
        {
            const EntityID lFrom = m_Shots[m_Shots.size() - 2].Camera;
            lResolution.BlendFrom = Entity(lFrom, &lWorld).IsValid() ? lFrom : ENTITY_NONE;
        }
        return lResolution;
    }

    CameraModifier& CameraDirector::AddModifier(TStrongObjectPtr<CameraModifier> InModifier)
    {
        CameraModifier& lAdded = *InModifier;
        m_Modifiers.push_back(Move(InModifier));
        return lAdded;
    }

    void CameraDirector::RemoveModifier(const CameraModifier& InModifier)
    {
        Algo::RemoveIf(m_Modifiers, [&InModifier](const TStrongObjectPtr<CameraModifier>& InOwned)
        {
            return InOwned.Get() == &InModifier;
        });
    }

    void CameraDirector::Cut()
    {
        if (m_Shots.size() > 1)
        {
            m_Shots.erase(m_Shots.begin(), m_Shots.end() - 1);
        }
        if (!m_Shots.empty())
        {
            m_Shots.front().Duration = 0.f;
            m_Shots.front().Curve    = ECameraBlend::Cut;
        }
    }
}
