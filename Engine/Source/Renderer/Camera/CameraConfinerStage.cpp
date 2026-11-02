#include "Renderer/Camera/CameraConfinerStage.h"

#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/CameraView.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/World.h"

namespace Opaax
{
    TOptional<Bounds2D> CameraConfinerStage::GetBounds()
    {
        if (BoundsEntity.IsEmpty())
        {
            return CameraConfinerBounds(Center, Size, nullptr);
        }

        if (!m_BoundsEntity.IsValid())
        {
            m_BoundsEntity = FindEntity(BoundsEntity);
            if (!m_BoundsEntity.IsValid())
            {
                return {};
            }
        }

        const TransformComponent lPlace = EntityHierarchy::WorldTransform(m_BoundsEntity);
        return CameraConfinerBounds(Center, Size, &lPlace);
    }

    void CameraConfinerStage::Step(CameraRigPose& InOutPose, float)
    {
        const TOptional<Bounds2D> lBounds = GetBounds();
        if (!lBounds.has_value())
        {
            return;
        }

        // A rolled view reaches further: its corners are kept inside too.
        const CameraComponent* lCamera = TryGet<CameraComponent>();
        const float            lRoll   = (lCamera != nullptr && lCamera->bRotateView) ? InOutPose.Rotation : 0.f;
        const Bounds2D         lView   = ViewBounds(CameraView{ InOutPose.Position, InOutPose.OrthoSize, lRoll },
                                                    GetWorld().GetViewAspect());

        InOutPose.Position = ConfineCameraView(InOutPose.Position, lView.HalfExtent, *lBounds);
    }
}
