#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Camera/CameraComponent.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // CameraProbe — records the view the world is framed with, every frame. With Raise, gives the
    //   camera named Raise priority 10, RaiseAfter seconds after it starts: a camera switch.
    // =============================================================================
    class CameraProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Raise;
        float              RaiseAfter = 0.5f;

        float ViewX    = 0.f;
        float ViewY    = 0.f;
        float ViewSize = 0.f;

        OPAAX_PROPERTIES(CameraProbe, OPAAX_PROP(Raise), OPAAX_PROP(RaiseAfter), OPAAX_PROP(ViewX), OPAAX_PROP(ViewY),
                         OPAAX_PROP(ViewSize))

        void OnStart() override
        {
            if (!Raise.IsEmpty())
            {
                SetTimer<&CameraProbe::RaiseCamera>(RaiseAfter);
            }
            Read();
        }

        void OnUpdate(float) override { Read(); }

    private:
        void RaiseCamera()
        {
            Opaax::Entity lCamera = FindEntity(Raise);
            if (Opaax::CameraComponent* lComponent = lCamera.IsValid() ? lCamera.TryGet<Opaax::CameraComponent>() : nullptr)
            {
                lComponent->Priority = 10;
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
