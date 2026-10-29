#pragma once

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraManager.h"
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
    //   With ZoomPerStep, Chase also zooms out every step: while it frames the world, the view's size
    //   is as far through the camera's last step as its pose (MaxZoomLag near 0). Framed from the
    //   raw size it is always at the end of the step.
    //   With WrapAfter, the runner jumps back to where it started every WrapAfter seconds of steps, and
    //   Chase with it: a respawn, teleported and cut, so nothing slides across the level.
    //   With UpdateNudge, Chase is also moved up by that much in every Update (each step puts it back on
    //   the runner): a camera moved both in the steps and between them, reported as mixed.
    // =============================================================================
    class FollowProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(FollowProbe, Opaax::Behaviour)

    public:
        float              Speed  = 1800.f;
        Opaax::OpaaxString Chase;
        Opaax::Vector2F    Offset = { 0.f, 40.f };
        float              ZoomPerStep = 0.f;   // OrthoSize Chase gains every step
        float              WrapAfter   = 0.f;   // seconds of steps between two jumps back to the start; 0: never
        float              UpdateNudge = 0.f;   // how far Chase is also moved up in every Update; 0: not moved there

        Opaax::Int32 Frames        = 0;     // frames read
        Opaax::Int32 FramedBetween = 0;     // frames framed strictly inside the last step
        float        MinFraction   = 1.f;
        float        MaxFraction   = 0.f;
        float        ViewOffsetY   = 0.f;   // the view's height over the runner, last frame
        Opaax::Int32 ZoomFrames    = 0;     // frames read while Chase zooms and frames the world
        float        MaxZoomLag    = 0.f;   // largest gap between the zoom's and the pose's fraction of the step

        OPAAX_PROPERTIES(FollowProbe, OPAAX_PROP(Speed), OPAAX_PROP(Chase), OPAAX_PROP(Offset),
                         OPAAX_PROP(ZoomPerStep), OPAAX_PROP(WrapAfter), OPAAX_PROP(UpdateNudge), OPAAX_PROP(Frames),
                         OPAAX_PROP(FramedBetween), OPAAX_PROP(MinFraction), OPAAX_PROP(MaxFraction),
                         OPAAX_PROP(ViewOffsetY), OPAAX_PROP(ZoomFrames), OPAAX_PROP(MaxZoomLag))

        void OnStart() override
        {
            Opaax::TransformInterpolation::RecordSteps(GetEntity());
            if (const Opaax::TransformComponent* lLocal = TryGet<Opaax::TransformComponent>())
            {
                m_StartX = lLocal->Position.x;
            }

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

            m_SinceWrap += InFixedDeltaTime;
            const bool bWrapped = WrapAfter > 0.f && m_SinceWrap >= WrapAfter;
            if (bWrapped)
            {
                lLocal->Position.x = m_StartX;
                m_SinceWrap        = 0.f;
            }

            if (m_Camera.IsValid())
            {
                m_Camera.Get<Opaax::TransformComponent>().Position = lLocal->Position + Offset;
                m_Camera.Get<Opaax::CameraComponent>().OrthoSize  += ZoomPerStep;
            }

            // Placed: the runner teleported, the cameras cut.
            if (bWrapped)
            {
                Opaax::TransformInterpolation::Teleport(GetEntity());
                Opaax::CameraManager::Cut(GetWorld());
            }
        }

        // Before this frame's steps: the runner's last step is the one the last frame was drawn in.
        void OnUpdate(float) override
        {
            // Up only: nudges that went back down would cancel out over an even number of frames.
            if (UpdateNudge != 0.f && m_Camera.IsValid())
            {
                m_Camera.Get<Opaax::TransformComponent>().Position.y += UpdateNudge;
            }

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

            // The zoom, while Chase frames the world (the view at its offset).
            const Opaax::CameraInterpolationComponent* lSizes =
                m_Camera.IsValid() ? m_Camera.TryGet<Opaax::CameraInterpolationComponent>() : nullptr;
            if (lSizes == nullptr || !lSizes->bHasStep || Opaax::Maths::Abs(ViewOffsetY - Offset.y) > 0.01f)
            {
                return;
            }

            const float lZoomStep = lSizes->SteppedOrthoSize - lSizes->PreviousOrthoSize;
            if (lZoomStep == 0.f)
            {
                return;
            }

            const float lZoomFraction = (GetWorld().GetCameraView().OrthoSize - lSizes->PreviousOrthoSize) / lZoomStep;

            ++ZoomFrames;
            MaxZoomLag = Opaax::Maths::Max(MaxZoomLag, Opaax::Maths::Abs(lZoomFraction - lFraction));
        }

    private:
        Opaax::Entity m_Camera;
        float         m_StartX    = 0.f;
        float         m_SinceWrap = 0.f;
    };
}
