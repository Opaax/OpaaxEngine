#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Camera/CameraComponent.h"
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
}
