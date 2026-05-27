#include "ShakeCameraController.h"

#include "ICamera.h"

#include <cmath>

namespace Opaax
{
    namespace
    {
        constexpr float k_TwoPi = 6.28318530717958647692f;

        // Decay envelope in [0..1] over t in [0..1]. Always 1 at t = 0; reaches 0 at t = 1
        // for finite-tail curves, holds 1 for Constant.
        float DecayFactor(EShakeDecay InDecay, float InNormT)
        {
            if (InNormT <= 0.f) return 1.f;
            if (InNormT >= 1.f) return (InDecay == EShakeDecay::Constant) ? 1.f : 0.f;

            switch (InDecay)
            {
                case EShakeDecay::Linear:   return 1.f - InNormT;
                case EShakeDecay::EaseOut:  { const float k = 1.f - InNormT; return k * k; }
                case EShakeDecay::Constant: return 1.f;
                default:                    return 1.f - InNormT;
            }
        }
    }

    ShakeCameraController::ShakeCameraController(const ShakeParams& InParams)
        : m_Params(InParams)
    {}

    void ShakeCameraController::Tick(ICamera& InCamera, double InDeltaSeconds)
    {
        if (m_Params.Duration <= 0.f || m_Params.Amplitude == 0.f)
        {
            // Mark expired immediately — controller will be pruned this frame.
            m_Elapsed = (m_Params.Duration > 0.f) ? m_Params.Duration : 1.f;
            return;
        }

        m_Elapsed += static_cast<float>(InDeltaSeconds);

        const float lNormT  = m_Elapsed / m_Params.Duration;
        const float lDecay  = DecayFactor(m_Params.Decay, lNormT);
        const float lAmp    = m_Params.Amplitude * lDecay;
        if (lAmp == 0.f)
        {
            return;
        }

        const float lOmegaX = k_TwoPi * m_Params.Frequency;
        const float lOmegaY = lOmegaX * m_Params.YFrequencyRatio;

        const Vector2F lOffset{
            std::sin(lOmegaX * m_Elapsed)                    * lAmp,
            std::sin(lOmegaY * m_Elapsed + m_Params.YPhase)  * lAmp
        };

        InCamera.AddPositionOffset(lOffset);
    }

    bool ShakeCameraController::IsFinished() const
    {
        return m_Params.Duration <= 0.f || m_Elapsed >= m_Params.Duration;
    }

} // namespace Opaax
