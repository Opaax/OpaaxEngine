#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraShakeSubsystem.h"
#include "World/Behaviour/Behaviour.h"
#include "World/World.h"

namespace TestWorld
{
    // =============================================================================
    // CameraProbe — records the view the world is framed with, every frame. With Raise, gives the
    //   camera named Raise priority RaiseTo, RaiseAfter seconds after it starts: a camera switch. With
    //   RaiseOnStep, in the fixed step after the world's RaiseOnStep-th instead: a switch made in a
    //   step, drawn the same at any frame rate.
    // =============================================================================
    class CameraProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(CameraProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Raise;
        float              RaiseAfter  = 0.5f;
        Opaax::Int32       RaiseOnStep = -1;   // -1: on the RaiseAfter timer
        Opaax::Int32       RaiseTo     = 10;

        float ViewX    = 0.f;
        float ViewY    = 0.f;
        float ViewSize = 0.f;
        bool  bRaised  = false;

        OPAAX_PROPERTIES(CameraProbe, OPAAX_PROP(Raise), OPAAX_PROP(RaiseAfter), OPAAX_PROP(RaiseOnStep),
                         OPAAX_PROP(RaiseTo), OPAAX_PROP(ViewX), OPAAX_PROP(ViewY), OPAAX_PROP(ViewSize),
                         OPAAX_PROP(bRaised))

        void OnStart() override
        {
            if (!Raise.IsEmpty() && RaiseOnStep < 0)
            {
                SetTimer<&CameraProbe::RaiseCamera>(RaiseAfter);
            }
            Read();
        }

        void OnUpdate(float) override { Read(); }

        void OnFixedUpdate(float) override
        {
            if (!bRaised && RaiseOnStep >= 0 && GetWorld().GetStepCount() >= static_cast<Opaax::Uint64>(RaiseOnStep))
            {
                RaiseCamera();
            }
        }

    private:
        void RaiseCamera()
        {
            Opaax::Entity lCamera = FindEntity(Raise);
            if (Opaax::CameraComponent* lComponent = lCamera.IsValid() ? lCamera.TryGet<Opaax::CameraComponent>() : nullptr)
            {
                lComponent->Priority = RaiseTo;
                bRaised              = true;
            }
        }

        void Read()
        {
            const Opaax::CameraView& lView = GetWorld().GetCameraView();
            ViewX    = lView.Position.x;
            ViewY    = lView.Position.y;
            ViewSize = lView.OrthoSize;
        }
    };

    // =============================================================================
    // ShakeProbe — on its entity's CameraShakeSource: plays it in the fixed step after the world's
    //   PlayOnStep-th, and raises its trauma by Trauma in the one after TraumaOnStep (-1: never). Made
    //   in steps, so drawn the same at any frame rate. Records the trauma at each step.
    // =============================================================================
    class ShakeProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(ShakeProbe, Opaax::Behaviour)

    public:
        Opaax::Int32 PlayOnStep   = -1;
        Opaax::Int32 TraumaOnStep = -1;
        float        Trauma       = 1.f;

        bool  bPlayed   = false;
        bool  bHit      = false;
        float TraumaNow = 0.f;

        OPAAX_PROPERTIES(ShakeProbe, OPAAX_PROP(PlayOnStep), OPAAX_PROP(TraumaOnStep), OPAAX_PROP(Trauma),
                         OPAAX_PROP(bPlayed), OPAAX_PROP(bHit), OPAAX_PROP(TraumaNow))

        void OnFixedUpdate(float) override
        {
            Opaax::CameraShakeSubsystem* lShakes = GetWorld().GetSubsystems().GetSubsystem<Opaax::CameraShakeSubsystem>();
            if (lShakes == nullptr)
            {
                return;
            }

            const Opaax::Uint64 lStep = GetWorld().GetStepCount();
            if (!bPlayed && PlayOnStep >= 0 && lStep >= static_cast<Opaax::Uint64>(PlayOnStep))
            {
                bPlayed = lShakes->PlaySource(GetEntity().GetHandle()) != 0;
            }

            if (!bHit && TraumaOnStep >= 0 && lStep >= static_cast<Opaax::Uint64>(TraumaOnStep))
            {
                lShakes->AddTrauma(GetEntity().GetHandle(), Trauma);
                bHit = true;
            }

            TraumaNow = lShakes->GetTrauma(GetEntity().GetHandle());
        }
    };
}
