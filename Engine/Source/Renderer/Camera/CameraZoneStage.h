#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectMacros.h"
#include "Renderer/Camera/CameraBlend.h"
#include "Renderer/Camera/CameraConfinerStage.h"
#include "Renderer/Camera/CameraStage.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // CameraZone — a zone as a zone stage weighs it (CameraZoneComponent, placed in the world).
    // =============================================================================
    struct CameraZone
    {
        EntityID Entity    = ENTITY_NONE;
        Bounds2D Bounds;
        float    OrthoSize = 0.f;
        bool     bConfine  = true;
        Int32    Priority  = 0;
        float    BlendTime = 0.5f;
    };

    // =============================================================================
    // CameraZoneState — what a zone stage carries from step to step: the zone applied, and the blend
    //   into it (how far the view was from the zone's own framing when it started to apply, faded out).
    // =============================================================================
    struct CameraZoneState
    {
        TOptional<CameraZone> Zone;
        float                 BlendTime  = 0.f;
        float                 Elapsed    = 0.f;
        Vector2F              Offset     = { 0.f, 0.f };
        float                 SizeOffset = 0.f;
    };

    // =============================================================================
    // Zones (pure functions)
    // =============================================================================
    /** How far the blend into the zone applied is: 0 where it started, 1 done. */
    inline float CameraZoneBlendWeight(const CameraZoneState& InState)
    {
        return InState.BlendTime > 0.f ? CameraBlendWeight(ECameraBlend::EaseInOut, InState.Elapsed / InState.BlendTime)
                                       : 1.f;
    }

    /** The highest-priority zone of InZones holding InPoint (among equals, the first), or null. */
    inline const CameraZone* PickCameraZone(const TDynArray<CameraZone>& InZones, const Vector2F InPoint)
    {
        const CameraZone* lBest = nullptr;
        for (const CameraZone& lZone : InZones)
        {
            if (lZone.Bounds.Contains(InPoint) && (lBest == nullptr || lZone.Priority > lBest->Priority))
            {
                lBest = &lZone;
            }
        }
        return lBest;
    }

    namespace CameraZoneDetail
    {
        /**
         * InGiven framed by InZone (none: as given): its OrthoSize, then the upright view confined to it,
         * each off the zone's own framing by its offset times 1 - InWeight.
         */
        inline CameraRigPose Frame(const CameraRigPose& InGiven, const TOptional<CameraZone>& InZone,
                                   const Vector2F InOffset, const float InSizeOffset, const float InWeight,
                                   const float InAspect)
        {
            const float   lFade = 1.f - InWeight;
            CameraRigPose lPose = InGiven;

            lPose.OrthoSize = ((InZone.has_value() && InZone->OrthoSize > 0.f) ? InZone->OrthoSize : InGiven.OrthoSize)
                            + InSizeOffset * lFade;
            if (InZone.has_value() && InZone->bConfine)
            {
                lPose.Position = ConfineCameraView(InGiven.Position, { lPose.OrthoSize * InAspect, lPose.OrthoSize },
                                                   InZone->Bounds);
            }
            lPose.Position += InOffset * lFade;
            return lPose;
        }
    }

    /**
     * Applies InZone (null: none) to InOutPose for a step of InDeltaTime: its OrthoSize (0 keeps the
     * size given), then the view kept inside it. When the zone applied changes, the view blends from
     * where the last one put it over the new zone's BlendTime (the old one's when leaving every zone),
     * eased in and out, so it never jumps.
     * @param InAspect The screen's width over height
     */
    inline void StepCameraZone(CameraRigPose& InOutPose, const CameraZone* InZone, CameraZoneState& InOutState,
                               const float InAspect, const float InDeltaTime)
    {
        InOutState.Elapsed += InDeltaTime;
        const float lWeight = CameraZoneBlendWeight(InOutState);

        const bool bSame = InZone != nullptr ? (InOutState.Zone.has_value() && InOutState.Zone->Entity == InZone->Entity)
                                             : !InOutState.Zone.has_value();
        if (bSame)
        {
            // Moved or changed, the zone is followed.
            if (InZone != nullptr)
            {
                InOutState.Zone = *InZone;
            }
            InOutPose = CameraZoneDetail::Frame(InOutPose, InOutState.Zone, InOutState.Offset, InOutState.SizeOffset,
                                                lWeight, InAspect);
            return;
        }

        // Another zone: the blend starts where the last one puts the view now.
        const CameraRigPose lGiven = InOutPose;
        const CameraRigPose lFrom = CameraZoneDetail::Frame(lGiven, InOutState.Zone, InOutState.Offset,
                                                            InOutState.SizeOffset, lWeight, InAspect);
        const TOptional<CameraZone> lNext = InZone != nullptr ? TOptional<CameraZone>(*InZone) : TOptional<CameraZone>();

        const float         lNextSize   = (lNext.has_value() && lNext->OrthoSize > 0.f) ? lNext->OrthoSize : lGiven.OrthoSize;
        const float         lSizeOffset = lFrom.OrthoSize - lNextSize;
        const CameraRigPose lNextFrame  = CameraZoneDetail::Frame(lGiven, lNext, { 0.f, 0.f }, lSizeOffset, 0.f, InAspect);

        InOutState.BlendTime  = lNext.has_value() ? lNext->BlendTime
                                                  : (InOutState.Zone.has_value() ? InOutState.Zone->BlendTime : 0.f);
        InOutState.Elapsed    = 0.f;
        InOutState.Offset     = lFrom.Position - lNextFrame.Position;
        InOutState.SizeOffset = lSizeOffset;
        InOutState.Zone       = lNext;
        InOutPose             = lFrom;

        // A cut applies from this step.
        if (InOutState.BlendTime <= 0.f)
        {
            InOutState.Offset     = { 0.f, 0.f };
            InOutState.SizeOffset = 0.f;
            InOutPose = CameraZoneDetail::Frame(lGiven, lNext, { 0.f, 0.f }, 0.f, 1.f, InAspect);
        }
    }

    // =============================================================================
    // CameraZoneStage — a Constraint stage that applies the level's zones (CameraZoneComponent) to
    //   its camera (StepCameraZone): the highest-priority zone holding Target sets the camera's size
    //   and keeps the view inside it, blending in over its BlendTime. A level that zooms in a room and
    //   out again gives the outside a zone too (a large one, of a lower priority).
    // =============================================================================
    class CameraZoneStage final : public CameraStage
    {
        OPAAX_OBJECT(CameraZoneStage, CameraStage)

    public:
        /** The entity whose zone applies, by name; empty: the camera's own position. */
        OpaaxString Target;

        OPAAX_PROPERTIES(CameraZoneStage,
                         OPAAX_PROP(Target).SetTooltip("The entity whose zone applies, by name.\n"
                                                       "Empty: where the camera is."))

        /** The zone applied and the blend into it. */
        const CameraZoneState& GetState() const noexcept { return m_State; }

        //~Begin CameraStage interface
        ECameraStage GetStage() const override { return ECameraStage::Constraint; }
        void         Step(CameraRigPose& InOutPose, float InDeltaTime) override;
        //~End CameraStage interface

    private:
        Entity                m_Target;
        CameraZoneState       m_State;
        TDynArray<CameraZone> m_Zones;
    };
}
