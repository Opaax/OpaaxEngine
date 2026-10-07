#include "Renderer/Lighting/Lighting2D.h"

#include <algorithm>
#include <cmath>

#include "Core/Maths/Maths.h"
#include "Renderer/Components/Light2DComponent.h"

namespace Opaax
{
    namespace
    {
        /** Global lights rank above every point and spot light. */
        constexpr float GLOBAL_LIGHT_SCORE = 1.0e30f;

        struct Candidate
        {
            const Light2DInstance* Instance = nullptr;
            float                  Score    = 0.f;
        };

        bool Reaches(const Light2DInstance& InLight, const Bounds2D& InView)
        {
            if (InLight.Light->Type == ELight2DType::Global)
            {
                return true;
            }

            // The circle touches the box when the box's nearest point to its centre is within the radius.
            const Vector2F lMin = InView.Center - InView.HalfExtent;
            const Vector2F lMax = InView.Center + InView.HalfExtent;
            const Vector2F lNearest{ std::clamp(InLight.Position.x, lMin.x, lMax.x),
                                     std::clamp(InLight.Position.y, lMin.y, lMax.y) };
            const Vector2F lOffset = InLight.Position - lNearest;
            const float    lRadius = std::max(InLight.Light->Radius, 0.f);

            return lOffset.x * lOffset.x + lOffset.y * lOffset.y <= lRadius * lRadius;
        }

        /** How much a light matters to the view: strong, far-reaching and near the view rank first. */
        float Score(const Light2DInstance& InLight, const Bounds2D& InView)
        {
            const Light2DComponent& lLight = *InLight.Light;
            if (lLight.Type == ELight2DType::Global)
            {
                return GLOBAL_LIGHT_SCORE * lLight.Intensity;
            }

            const Vector2F lOffset   = InLight.Position - InView.Center;
            const float    lDistance = std::sqrt(lOffset.x * lOffset.x + lOffset.y * lOffset.y);
            const float    lRadius   = std::max(lLight.Radius, 1.f);
            return lLight.Intensity * lRadius / (lDistance + lRadius);
        }

        float TypeCode(const ELight2DType InType) noexcept
        {
            switch (InType)
            {
            case ELight2DType::Point:  return LIGHT_CODE_POINT;
            case ELight2DType::Spot:   return LIGHT_CODE_SPOT;
            case ELight2DType::Global: return LIGHT_CODE_GLOBAL;
            }
            return LIGHT_CODE_POINT;
        }
    }

    float ScreenToLinear(const float InChannel) noexcept
    {
        // The same approximation as the shaders' decode.
        return std::pow(std::max(InChannel, 0.f), 2.2f);
    }

    Uint32 PackLights2D(const TDynArray<Light2DInstance>& InLights, const Bounds2D& InView,
                        const Vector3F& InAmbientColor, const float InAmbientIntensity, LightsBlock2D& OutBlock)
    {
        const float lAmbient = std::max(InAmbientIntensity, 0.f);

        OutBlock = LightsBlock2D{};
        OutBlock.Ambient = Vector4F{ ScreenToLinear(InAmbientColor.x) * lAmbient, ScreenToLinear(InAmbientColor.y) * lAmbient,
                                     ScreenToLinear(InAmbientColor.z) * lAmbient, 0.f };

        TDynArray<Candidate> lCandidates;
        lCandidates.reserve(InLights.size());
        for (const Light2DInstance& lInstance : InLights)
        {
            const Light2DComponent* lLight = lInstance.Light;
            if (lLight == nullptr || !lLight->bEnabled || lLight->Intensity <= 0.f || !Reaches(lInstance, InView))
            {
                continue;
            }

            lCandidates.push_back(Candidate{ &lInstance, Score(lInstance, InView) });
        }

        const Uint32 lReaching = static_cast<Uint32>(lCandidates.size());
        if (lReaching > MAX_LIGHTS_2D)
        {
            std::stable_sort(lCandidates.begin(), lCandidates.end(),
                             [](const Candidate& InA, const Candidate& InB) { return InA.Score > InB.Score; });
            lCandidates.resize(MAX_LIGHTS_2D);
        }

        for (Uint32 lIndex = 0; lIndex < static_cast<Uint32>(lCandidates.size()); ++lIndex)
        {
            const Light2DInstance&  lInstance = *lCandidates[lIndex].Instance;
            const Light2DComponent& lLight    = *lInstance.Light;

            const float lRotation = Maths::DegreesToRadians(lInstance.RotationDegrees);

            // A spot fades from its inner half-angle (full light) to its outer one (none).
            const float lHalfOuter = Maths::DegreesToRadians(std::clamp(lLight.ConeAngle, 1.f, 360.f) * 0.5f);
            const float lHalfInner = lHalfOuter * (1.f - std::clamp(lLight.ConeSoftness, 0.f, 1.f));

            const float lHeight = (lLight.Type == ELight2DType::Global)
                                      ? std::sin(Maths::DegreesToRadians(std::clamp(lLight.Elevation, 1.f, 90.f)))
                                      : std::max(lLight.Height, 1.f);

            OutBlock.Position[lIndex]  = Vector4F{ lInstance.Position.x, lInstance.Position.y,
                                                   std::max(lLight.Radius, 1.f), TypeCode(lLight.Type) };
            OutBlock.Color[lIndex]     = Vector4F{ ScreenToLinear(lLight.Color.r) * lLight.Intensity,
                                                   ScreenToLinear(lLight.Color.g) * lLight.Intensity,
                                                   ScreenToLinear(lLight.Color.b) * lLight.Intensity,
                                                   std::max(lLight.Falloff, 0.01f) };
            OutBlock.Direction[lIndex] = Vector4F{ std::cos(lRotation), std::sin(lRotation),
                                                   std::cos(lHalfOuter), std::cos(lHalfInner) };
            OutBlock.Params[lIndex]    = Vector4F{ lHeight, -1.f, 0.f, 0.f };
        }

        OutBlock.Ambient.w = static_cast<float>(lCandidates.size());
        return lReaching;
    }
}
