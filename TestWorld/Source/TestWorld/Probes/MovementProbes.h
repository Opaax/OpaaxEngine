#pragma once

#include <algorithm>
#include <cmath>

#include "Movement/MoverComponent.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // WalkerProbe — drives its entity's Mover through a routine timed in physics steps (the same at
    //   any frame rate): rest until WalkAt, walk right until JumpAt, jump, then switch to the "Fly"
    //   mode at FlyAt and rise. Records what the mover did at each stage.
    // =============================================================================
    class WalkerProbe : public Opaax::Behaviour
    {
    public:
        float WalkAt = 0.5f;
        float JumpAt = 2.0f;
        float FlyAt  = 2.8f;

        bool  bRested    = false;   // grounded when the walk began
        float RestY      = 0.f;
        float PeakSpeed  = 0.f;
        float StoppedX   = 0.f;     // where the walk ended
        float JumpHeight = 0.f;     // the jump's peak above RestY
        float FlyRise    = 0.f;     // how far it rose since the switch to Fly

        OPAAX_PROPERTIES(WalkerProbe, OPAAX_PROP(WalkAt), OPAAX_PROP(JumpAt), OPAAX_PROP(FlyAt), OPAAX_PROP(bRested),
                         OPAAX_PROP(RestY), OPAAX_PROP(PeakSpeed), OPAAX_PROP(StoppedX), OPAAX_PROP(JumpHeight),
                         OPAAX_PROP(FlyRise))

        void OnFixedUpdate(const float InFixedDeltaTime) override
        {
            Opaax::MoverComponent* const lMover = TryGet<Opaax::MoverComponent>();
            if (lMover == nullptr)
            {
                return;
            }

            const Opaax::Vector2F lHere = GetPosition();

            if (m_Time < WalkAt)
            {
                lMover->Input.MoveDir = { 0.f, 0.f };
            }
            else if (m_Time < JumpAt)
            {
                if (m_Stage == 0)
                {
                    m_Stage = 1;
                    bRested = lMover->bGrounded;
                    RestY   = lHere.y;
                }
                lMover->Input.MoveDir = { 1.f, 0.f };
                PeakSpeed = std::max(PeakSpeed, std::abs(lMover->Velocity.x));
            }
            else if (m_Time < FlyAt)
            {
                if (m_Stage == 1)
                {
                    m_Stage  = 2;
                    StoppedX = lHere.x;
                    lMover->Input.bJump = true;
                }
                lMover->Input.MoveDir = { 0.f, 0.f };
                JumpHeight = std::max(JumpHeight, lHere.y - RestY);
            }
            else
            {
                if (m_Stage == 2)
                {
                    m_Stage     = 3;
                    m_FlyStartY = lHere.y;
                    lMover->PendingMode = OPAAX_ID("Fly");
                }
                lMover->Input.MoveDir = { 0.f, 1.f };
                FlyRise = lHere.y - m_FlyStartY;
            }

            m_Time += InFixedDeltaTime;
        }

    private:
        float        m_Time      = 0.f;
        Opaax::Int32 m_Stage     = 0;
        float        m_FlyStartY = 0.f;
    };
}
