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
            OutBlock.Params[lIndex]    = Vector4F{ lHeight, -1.f, std::max(lLight.ShadowSoftness, 0.f),
                                                   std::clamp(lLight.ShadowStrength, 0.f, 1.f) };
        }

        // Shadow map rows: the strongest shadowed point and spot lights first.
        const Uint32      lKept = static_cast<Uint32>(lCandidates.size());
        TDynArray<Uint32> lShadowed;
        for (Uint32 lIndex = 0; lIndex < lKept; ++lIndex)
        {
            const Light2DComponent& lLight = *lCandidates[lIndex].Instance->Light;
            if (lLight.bCastShadows && lLight.Type != ELight2DType::Global)
            {
                lShadowed.push_back(lIndex);
            }
        }

        std::stable_sort(lShadowed.begin(), lShadowed.end(), [&lCandidates](const Uint32 InA, const Uint32 InB)
        {
            return lCandidates[InA].Score > lCandidates[InB].Score;
        });

        const Uint32 lRows = std::min(static_cast<Uint32>(lShadowed.size()), MAX_SHADOWED_LIGHTS_2D);
        for (Uint32 lRow = 0; lRow < lRows; ++lRow)
        {
            OutBlock.Params[lShadowed[lRow]].y = static_cast<float>(lRow);
        }

        OutBlock.Ambient.w = static_cast<float>(lKept);
        return lReaching;
    }

    void ClearShadowRows2D(LightsBlock2D& InOutBlock) noexcept
    {
        for (Uint32 lIndex = 0; lIndex < InOutBlock.GetCount() && lIndex < MAX_LIGHTS_2D; ++lIndex)
        {
            InOutBlock.Params[lIndex].y = -1.f;
        }
    }

    Uint32 BuildShadowBlock2D(const LightsBlock2D& InLights, ShadowBlock2D& OutBlock) noexcept
    {
        OutBlock = ShadowBlock2D{};

        Uint32 lRows = 0;
        for (Uint32 lIndex = 0; lIndex < InLights.GetCount() && lIndex < MAX_LIGHTS_2D; ++lIndex)
        {
            const float lRow = InLights.Params[lIndex].y;
            if (lRow < 0.f || lRow >= static_cast<float>(MAX_SHADOWED_LIGHTS_2D))
            {
                continue;
            }

            const Uint32    lRowIndex = static_cast<Uint32>(lRow);
            const Vector4F& lLight    = InLights.Position[lIndex];
            OutBlock.Light[lRowIndex] = Vector4F{ lLight.x, lLight.y, lLight.z, 0.f };
            lRows = std::max(lRows, lRowIndex + 1u);
        }

        OutBlock.Info.x = static_cast<float>(lRows);
        return lRows;
    }

    OcclusionLayout2D MakeOcclusionLayout2D(const Bounds2D& InView, const float InPixelsPerUnit,
                                            const ShadowBlock2D& InShadows) noexcept
    {
        // The view, grown to take in what the shadowed lights reach (casters just off screen cast into it).
        Vector2F lMin = InView.Min();
        Vector2F lMax = InView.Max();
        for (Uint32 lRow = 0; lRow < InShadows.GetCount() && lRow < MAX_SHADOWED_LIGHTS_2D; ++lRow)
        {
            const Vector4F& lLight = InShadows.Light[lRow];
            lMin = Vector2F{ std::min(lMin.x, lLight.x - lLight.z), std::min(lMin.y, lLight.y - lLight.z) };
            lMax = Vector2F{ std::max(lMax.x, lLight.x + lLight.z), std::max(lMax.y, lLight.y + lLight.z) };
        }

        // At most a few times the view: the shadows of farther casters are not worth the pixels.
        const Vector2F lLimit = InView.HalfExtent * OCCLUSION_MAX_SCALE_2D;
        lMin = Vector2F{ std::max(lMin.x, InView.Center.x - lLimit.x), std::max(lMin.y, InView.Center.y - lLimit.y) };
        lMax = Vector2F{ std::min(lMax.x, InView.Center.x + lLimit.x), std::min(lMax.y, InView.Center.y + lLimit.y) };

        const Vector2F lSize{ std::max(lMax.x - lMin.x, 0.f), std::max(lMax.y - lMin.y, 0.f) };

        // Screen density, unless the map would grow past its largest size.
        const float lLargest = std::max(lSize.x, lSize.y);
        float       lDensity = (InPixelsPerUnit > 0.f) ? InPixelsPerUnit : 1.f;
        if (lLargest * lDensity > static_cast<float>(OCCLUSION_MAX_SIZE_2D))
        {
            lDensity = static_cast<float>(OCCLUSION_MAX_SIZE_2D) / lLargest;
        }

        // Rounded up, past a float's noise (1000 * 1.2f is a hair above 1200).
        const auto lPixels = [lDensity](const float InExtent)
        {
            const float lExact = std::ceil(InExtent * lDensity - 1.0e-3f);
            return std::clamp(static_cast<Uint32>(std::max(lExact, 0.f)), 1u, OCCLUSION_MAX_SIZE_2D);
        };

        OcclusionLayout2D lLayout;
        lLayout.Width  = lPixels(lSize.x);
        lLayout.Height = lPixels(lSize.y);

        // Whole pixels: a view of Height pixels over HalfExtent.y gives exactly Width over HalfExtent.x.
        lLayout.Bounds.Center     = (lMin + lMax) * 0.5f;
        lLayout.Bounds.HalfExtent = Vector2F{ static_cast<float>(lLayout.Width), static_cast<float>(lLayout.Height) }
                                  / (2.f * lDensity);
        return lLayout;
    }
}
