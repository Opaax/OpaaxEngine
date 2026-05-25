#include "FollowCameraController.h"

#include "ICamera.h"

#include "World/World.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>

namespace Opaax
{
    FollowCameraController::FollowCameraController(const FollowParams& InParams, World& InWorld)
        : m_Params(InParams)
        , m_World(InWorld)
    {}

    void FollowCameraController::Tick(ICamera& InCamera, double InDeltaSeconds)
    {
        if (m_Params.Target == ENTITY_NONE)
        {
            return;
        }

        const ECS::TransformComponent* lTransform =
            m_World.GetComponent<ECS::TransformComponent>(m_Params.Target);
        if (!lTransform)
        {
            return;
        }

        const Vector2F lTargetWorld = lTransform->Position + m_Params.Offset;
        const Vector2F lCamPos      = InCamera.GetPosition();
        const Vector2F lDelta       = lTargetWorld - lCamPos;

        // Deadzone — camera holds still while target is inside the box around target+offset.
        if (std::abs(lDelta.x) <= m_Params.Deadzone.x &&
            std::abs(lDelta.y) <= m_Params.Deadzone.y)
        {
            return;
        }

        // Snap when smoothing is effectively zero.
        if (m_Params.Smoothing <= 0.f)
        {
            InCamera.SetPosition(lTargetWorld);
            return;
        }

        // Frame-rate-independent exponential smoothing. tau = Smoothing seconds.
        const float    lAlpha  = 1.f - std::exp(-static_cast<float>(InDeltaSeconds) / m_Params.Smoothing);
        const Vector2F lNewPos = lCamPos + lDelta * lAlpha;
        InCamera.SetPosition(lNewPos);
    }

} // namespace Opaax
