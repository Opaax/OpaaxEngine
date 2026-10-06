#include "Input/Mapping/InputModifiers.h"

#include <cmath>

namespace Opaax
{
    namespace
    {
        float Magnitude(Vector2F InValue) noexcept
        {
            return std::sqrt(InValue.x * InValue.x + InValue.y * InValue.y);
        }
    }

    namespace InputModifiers
    {
        Vector2F Apply(Vector2F InValue, const InputModifierData& InModifier) noexcept
        {
            switch (InModifier.Type)
            {
            case EInputModifier::Negate:
                return -InValue;

            case EInputModifier::Swizzle:
                // A swap, so it is its own inverse.
                return Vector2F{InValue.y, InValue.x};

            case EInputModifier::Scalar:
                return Vector2F{InValue.x * InModifier.Scale.x, InValue.y * InModifier.Scale.y};

            case EInputModifier::Normalize:
            {
                // Shrink only: a slightly pressed stick must not read as fully pressed.
                const float lLength = Magnitude(InValue);
                if (lLength <= 1.f || lLength == 0.f)
                {
                    return InValue;
                }

                return InValue / lLength;
            }

            case EInputModifier::DeadZone:
            {
                // Radial dead zone (per-component would let diagonals through).
                const float lLength = Magnitude(InValue);
                if (lLength == 0.f)
                {
                    return InValue;
                }

                const float lLower = InModifier.DeadZoneLower;
                const float lUpper = InModifier.DeadZoneUpper;

                if (lLength <= lLower)
                {
                    return Vector2F{0.f, 0.f};
                }

                const Vector2F lDirection = InValue / lLength;

                if (lLength >= lUpper || lUpper <= lLower)
                {
                    return lDirection;
                }

                return lDirection * ((lLength - lLower) / (lUpper - lLower));
            }
            }

            return InValue;
        }

        Vector2F ApplyAll(Vector2F InValue, const TDynArray<InputModifierData>& InModifiers) noexcept
        {
            Vector2F lValue = InValue;

            for (const InputModifierData& lModifier : InModifiers)
            {
                lValue = Apply(lValue, lModifier);
            }

            return lValue;
        }
    }
}
