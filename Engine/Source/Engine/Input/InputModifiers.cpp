#include "Engine/Input/InputModifiers.h"

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
                // A SWAP, so it is its own inverse — which is what makes a WASD composite read
                // the same for the vertical pair as the horizontal one, plus a Negate.
                return Vector2F{InValue.y, InValue.x};

            case EInputModifier::Scalar:
                return Vector2F{InValue.x * InModifier.Scale.x, InValue.y * InModifier.Scale.y};

            case EInputModifier::Normalize:
            {
                // Shrink only. Growing a short vector to unit length would make a barely-pressed
                // stick read as fully pressed, which is the opposite of what this is for.
                const float lLength = Magnitude(InValue);
                if (lLength <= 1.f || lLength == 0.f)
                {
                    return InValue;
                }

                return InValue / lLength;
            }

            case EInputModifier::DeadZone:
            {
                // RADIAL, which is correct for a stick AND degrades to the right thing for one
                // axis, since the magnitude of (x, 0) is |x|. A per-component dead zone would let
                // a diagonal escape the zone that neither axis alone could.
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
