#pragma once

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/TransformInterpolation.h"

namespace TestWorld
{
    // =============================================================================
    // FollowProbe — runs its entity to the right at Speed every fixed step and puts the camera named
    //   Chase at Offset from it after each move: a camera moved in fixed steps. It asks the world to
    //   record its own steps, so it is drawn between them.
    //   Every frame it reads where the last frame was framed. With the camera straight above or below
    //   it, the view's x tells how far through the runner's last step that frame was drawn (0: the
    //   step before, 1: the last). Framed from the display pose, this is the frame's alpha: within
    //   [0, 1], and strictly inside on most frames. Framed from a raw pose it is a whole number.
    // =============================================================================
    class FollowProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(FollowProbe, Opaax::Behaviour)

    public:
        float              Speed  = 1800.f;
        Opaax::OpaaxString Chase;
        Opaax::Vector2F    Offset = { 0.f, 40.f };

        Opaax::Int32 Frames        = 0;     // frames read
        Opaax::Int32 FramedBetween = 0;     // frames framed strictly inside the last step
        float        MinFraction   = 1.f;
        float        MaxFraction   = 0.f;
        float        ViewOffsetY   = 0.f;   // the view's height over the runner, last frame

        OPAAX_PROPERTIES(FollowProbe, OPAAX_PROP(Speed), OPAAX_PROP(Chase), OPAAX_PROP(Offset), OPAAX_PROP(Frames),
                         OPAAX_PROP(FramedBetween), OPAAX_PROP(MinFraction), OPAAX_PROP(MaxFraction),
                         OPAAX_PROP(ViewOffsetY))

        void OnStart() override
        {
            Opaax::TransformInterpolation::RecordSteps(GetEntity());

            if (!Chase.IsEmpty())
            {
                m_Camera = FindEntity(Chase);
            }
        }

        void OnFixedUpdate(const float InFixedDeltaTime) override
        {
            Opaax::TransformComponent* const lLocal = TryGet<Opaax::TransformComponent>();
            if (lLocal == nullptr)
            {
                return;
            }

            lLocal->Position.x += Speed * InFixedDeltaTime;

            if (m_Camera.IsValid())
            {
                m_Camera.Get<Opaax::TransformComponent>().Position = lLocal->Position + Offset;
            }
        }

        // Before this frame's steps: the runner's last step is the one the last frame was drawn in.
        void OnUpdate(float) override
        {
            const Opaax::TransformComponent*              lLocal   = TryGet<Opaax::TransformComponent>();
            const Opaax::TransformInterpolationComponent* lHistory = TryGet<Opaax::TransformInterpolationComponent>();
            if (lLocal == nullptr || lHistory == nullptr || !lHistory->bHasPrevious || !lHistory->bHasStepped)
            {
                return;
            }

            const float lStep = lHistory->SteppedPosition.x - lHistory->Position.x;
            if (lStep == 0.f)
            {
                return;
            }

            const Opaax::Vector2F lView     = GetWorld().GetCameraView().Position;
            const float           lFraction = (lView.x - lHistory->Position.x) / lStep;

            ++Frames;
            MinFraction = Opaax::Maths::Min(MinFraction, lFraction);
            MaxFraction = Opaax::Maths::Max(MaxFraction, lFraction);

            if (lFraction > 0.001f && lFraction < 0.999f)
            {
                ++FramedBetween;
            }

            ViewOffsetY = lView.y - lLocal->Position.y;
        }

    private:
        Opaax::Entity m_Camera;
    };
}
